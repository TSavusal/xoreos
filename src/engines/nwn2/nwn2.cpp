/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  Engine class handling Neverwinter Nights 2
 */

#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/filelist.h"
#include "src/common/stream.h"
#include "src/common/configman.h"

#include "src/aurora/util.h"
#include "src/aurora/resman.h"
#include "src/aurora/talkman.h"
#include "src/aurora/talktable_tlk.h"

#include "src/graphics/aurora/cursorman.h"
#include "src/graphics/aurora/fontman.h"

#include "src/events/events.h"

#include "src/engines/aurora/util.h"
#include "src/engines/aurora/language.h"
#include "src/engines/aurora/loadprogress.h"
#include "src/engines/aurora/resources.h"
#include "src/engines/aurora/model.h"

#include "src/engines/nwn2/nwn2.h"
#include "src/engines/nwn2/modelloader.h"
#include "src/engines/nwn2/console.h"
#include "src/engines/nwn2/campaign.h"

namespace Engines {

namespace NWN2 {

const NWN2EngineProbe kNWN2EngineProbe;

const Common::UString NWN2EngineProbe::kGameName = "Neverwinter Nights 2";

NWN2EngineProbe::NWN2EngineProbe() {
}

NWN2EngineProbe::~NWN2EngineProbe() {
}

Aurora::GameID NWN2EngineProbe::getGameID() const {
	return Aurora::kGameIDNWN2;
}

const Common::UString &NWN2EngineProbe::getGameName() const {
	return kGameName;
}

bool NWN2EngineProbe::probe(const Common::UString &UNUSED(directory),
                            const Common::FileList &rootFiles) const {

	// If either the ini file or the binary is found, this should be a valid path
	if (rootFiles.contains("/nwn2.ini", true))
		return true;
	if (rootFiles.contains("/nwn2main.exe", true))
		return true;

	return false;
}

bool NWN2EngineProbe::probe(Common::SeekableReadStream &UNUSED(stream)) const {
	return false;
}

Engines::Engine *NWN2EngineProbe::createEngine() const {
	return new NWN2Engine;
}


NWN2Engine::NWN2Engine() : _language(Aurora::kLanguageInvalid),
	_hasXP1(false), _hasXP2(false), _hasXP3(false), _campaign(0) {

	_console = new Console(*this);
}

NWN2Engine::~NWN2Engine() {
	delete _campaign;
}

bool NWN2Engine::detectLanguages(Aurora::GameID game, const Common::UString &target,
                                 Aurora::Platform UNUSED(platform),
                                 std::vector<Aurora::Language> &languages) const {
	try {
		Common::FileList files;
		if (!files.addDirectory(target))
			return true;

		Common::UString tlk = files.findFirst("dialog.tlk", true);
		if (tlk.empty())
			return true;

		uint32 languageID = Aurora::TalkTable_TLK::getLanguageID(tlk);
		if (languageID == Aurora::kLanguageInvalid)
			return true;

		Aurora::Language language = Aurora::getLanguage(game, languageID);
		if (language == Aurora::kLanguageInvalid)
			return true;

		languages.push_back(language);

	} catch (...) {
	}

	return true;
}

bool NWN2Engine::getLanguage(Aurora::Language &language) const {
	language = _language;
	return true;
}

bool NWN2Engine::changeLanguage() {
	Aurora::Language language;
	if (!evaluateLanguage(false, language) || (_language != language))
		return false;

	return true;
}

Campaign *NWN2Engine::getCampaign() {
	return _campaign;
}

Module *NWN2Engine::getModule() {
	if (!_campaign)
		return 0;

	return _campaign->getModule();
}

void NWN2Engine::run() {
	init();
	if (EventMan.quitRequested())
		return;

	CursorMan.hideCursor();
	CursorMan.set();

	playIntroVideos();
	if (EventMan.quitRequested())
		return;

	CursorMan.showCursor();

	main();

	deinit();
}

void NWN2Engine::init() {
	LoadProgress progress(21);

	if (evaluateLanguage(true, _language))
		status("Setting the language to %s", Aurora::getLanguageName(_language).c_str());
	else
		warning("Failed to detect this game's language");

	progress.step("Loading user game config");
	initConfig();

	progress.step("Declare string encodings");
	declareEncodings();

	initResources(progress);
	if (EventMan.quitRequested())
		return;

	progress.step("Loading game cursors");
	initCursors();
	if (EventMan.quitRequested())
		return;

	progress.step("Initializing internal game config");
	initGameConfig();

	progress.step("Successfully initialized the engine");
}

void NWN2Engine::declareEncodings() {
	static const LanguageEncoding kLanguageEncodings[] = {
		{ Aurora::kLanguageEnglish           , Common::kEncodingCP1252 },
		{ Aurora::kLanguageFrench            , Common::kEncodingCP1252 },
		{ Aurora::kLanguageGerman            , Common::kEncodingCP1252 },
		{ Aurora::kLanguageItalian           , Common::kEncodingCP1252 },
		{ Aurora::kLanguageSpanish           , Common::kEncodingCP1252 },
		{ Aurora::kLanguagePolish            , Common::kEncodingCP1250 },
		{ Aurora::kLanguageKorean            , Common::kEncodingCP949  },
		{ Aurora::kLanguageChineseTraditional, Common::kEncodingCP950  },
		{ Aurora::kLanguageChineseSimplified , Common::kEncodingCP936  },
		{ Aurora::kLanguageJapanese          , Common::kEncodingCP932  }
	};

	Engines::declareEncodings(_game, kLanguageEncodings, ARRAYSIZE(kLanguageEncodings));
	Engines::declareTalkLanguage(_game, _language);
}

void NWN2Engine::initResources(LoadProgress &progress) {
	progress.step("Setting base directory");
	ResMan.registerDataBaseDir(_target);

	// NWN2's new file types overlap with other engines in the 3000s block
	ResMan.addTypeAlias((Aurora::FileType) 3000, Aurora::kFileTypeOSC);
	ResMan.addTypeAlias((Aurora::FileType) 3001, Aurora::kFileTypeUSC);
	ResMan.addTypeAlias((Aurora::FileType) 3002, Aurora::kFileTypeTRN);
	ResMan.addTypeAlias((Aurora::FileType) 3003, Aurora::kFileTypeUTR);
	ResMan.addTypeAlias((Aurora::FileType) 3004, Aurora::kFileTypeUEN);
	ResMan.addTypeAlias((Aurora::FileType) 3005, Aurora::kFileTypeULT);
	ResMan.addTypeAlias((Aurora::FileType) 3006, Aurora::kFileTypeSEF);
	ResMan.addTypeAlias((Aurora::FileType) 3007, Aurora::kFileTypePFX);
	ResMan.addTypeAlias((Aurora::FileType) 3008, Aurora::kFileTypeCAM);
	ResMan.addTypeAlias((Aurora::FileType) 3009, Aurora::kFileTypeLFX);
	ResMan.addTypeAlias((Aurora::FileType) 3010, Aurora::kFileTypeBFX);
	ResMan.addTypeAlias((Aurora::FileType) 3011, Aurora::kFileTypeUPE);
	ResMan.addTypeAlias((Aurora::FileType) 3012, Aurora::kFileTypeROS);
	ResMan.addTypeAlias((Aurora::FileType) 3013, Aurora::kFileTypeRST);
	ResMan.addTypeAlias((Aurora::FileType) 3014, Aurora::kFileTypeIFX);
	ResMan.addTypeAlias((Aurora::FileType) 3015, Aurora::kFileTypePFB);
	ResMan.addTypeAlias((Aurora::FileType) 3016, Aurora::kFileTypeZIP);
	ResMan.addTypeAlias((Aurora::FileType) 3017, Aurora::kFileTypeWMP);
	ResMan.addTypeAlias((Aurora::FileType) 3018, Aurora::kFileTypeBBX);
	ResMan.addTypeAlias((Aurora::FileType) 3019, Aurora::kFileTypeTFX);
	ResMan.addTypeAlias((Aurora::FileType) 3020, Aurora::kFileTypeWLK);
	ResMan.addTypeAlias((Aurora::FileType) 3021, Aurora::kFileTypeXML);
	ResMan.addTypeAlias((Aurora::FileType) 3022, Aurora::kFileTypeSCC);
	ResMan.addTypeAlias((Aurora::FileType) 3033, Aurora::kFileTypePTX);
	ResMan.addTypeAlias((Aurora::FileType) 3034, Aurora::kFileTypeLTX);
	ResMan.addTypeAlias((Aurora::FileType) 3035, Aurora::kFileTypeTRX);

	indexMandatoryDirectory("", 0, 0, 1);

	progress.step("Adding extra archive directories");
	ResMan.addArchiveDir(Aurora::kArchiveZIP, "data");
	ResMan.addArchiveDir(Aurora::kArchiveERF, "modules");
	ResMan.addArchiveDir(Aurora::kArchiveERF, "hak");

	progress.step("Loading main resource files");

	indexMandatoryArchive(Aurora::kArchiveZIP, "2da.zip"           , 10);
	indexMandatoryArchive(Aurora::kArchiveZIP, "actors.zip"        , 11);
	indexMandatoryArchive(Aurora::kArchiveZIP, "animtags.zip"      , 12);
	indexMandatoryArchive(Aurora::kArchiveZIP, "convo.zip"         , 13);
	indexMandatoryArchive(Aurora::kArchiveZIP, "ini.zip"           , 14);
	indexMandatoryArchive(Aurora::kArchiveZIP, "lod-merged.zip"    , 15);
	indexMandatoryArchive(Aurora::kArchiveZIP, "music.zip"         , 16);
	indexMandatoryArchive(Aurora::kArchiveZIP, "nwn2_materials.zip", 17);
	indexMandatoryArchive(Aurora::kArchiveZIP, "nwn2_models.zip"   , 18);
	indexMandatoryArchive(Aurora::kArchiveZIP, "nwn2_vfx.zip"      , 19);
	indexMandatoryArchive(Aurora::kArchiveZIP, "prefabs.zip"       , 20);
	indexMandatoryArchive(Aurora::kArchiveZIP, "scripts.zip"       , 21);
	indexMandatoryArchive(Aurora::kArchiveZIP, "sounds.zip"        , 22);
	indexMandatoryArchive(Aurora::kArchiveZIP, "soundsets.zip"     , 23);
	indexMandatoryArchive(Aurora::kArchiveZIP, "speedtree.zip"     , 24);
	indexMandatoryArchive(Aurora::kArchiveZIP, "templates.zip"     , 25);
	indexMandatoryArchive(Aurora::kArchiveZIP, "vo.zip"            , 26);
	indexMandatoryArchive(Aurora::kArchiveZIP, "walkmesh.zip"      , 27);

	progress.step("Loading expansion 1 resource files");

	// Expansion 1: Mask of the Betrayer (MotB)
	indexOptionalArchive(Aurora::kArchiveZIP, "2da_x1.zip"           , 50);
	indexOptionalArchive(Aurora::kArchiveZIP, "actors_x1.zip"        , 51);
	indexOptionalArchive(Aurora::kArchiveZIP, "animtags_x1.zip"      , 52);
	indexOptionalArchive(Aurora::kArchiveZIP, "convo_x1.zip"         , 53);
	indexOptionalArchive(Aurora::kArchiveZIP, "ini_x1.zip"           , 54);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_x1.zip"    , 55);
	indexOptionalArchive(Aurora::kArchiveZIP, "music_x1.zip"         , 56);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_x1.zip", 57);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_x1.zip"   , 58);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_vfx_x1.zip"      , 59);
	indexOptionalArchive(Aurora::kArchiveZIP, "prefabs_x1.zip"       , 60);
	indexOptionalArchive(Aurora::kArchiveZIP, "scripts_x1.zip"       , 61);
	indexOptionalArchive(Aurora::kArchiveZIP, "soundsets_x1.zip"     , 62);
	indexOptionalArchive(Aurora::kArchiveZIP, "sounds_x1.zip"        , 63);
	indexOptionalArchive(Aurora::kArchiveZIP, "speedtree_x1.zip"     , 64);
	indexOptionalArchive(Aurora::kArchiveZIP, "templates_x1.zip"     , 65);
	indexOptionalArchive(Aurora::kArchiveZIP, "vo_x1.zip"            , 66);
	indexOptionalArchive(Aurora::kArchiveZIP, "walkmesh_x1.zip"      , 67);

	progress.step("Loading expansion 2 resource files");

	// Expansion 2: Storm of Zehir (SoZ)
	indexOptionalArchive(Aurora::kArchiveZIP, "2da_x2.zip"           , 100);
	indexOptionalArchive(Aurora::kArchiveZIP, "actors_x2.zip"        , 101);
	indexOptionalArchive(Aurora::kArchiveZIP, "animtags_x2.zip"      , 102);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_x2.zip"    , 103);
	indexOptionalArchive(Aurora::kArchiveZIP, "music_x2.zip"         , 104);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_x2.zip", 105);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_x2.zip"   , 106);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_vfx_x2.zip"      , 107);
	indexOptionalArchive(Aurora::kArchiveZIP, "prefabs_x2.zip"       , 108);
	indexOptionalArchive(Aurora::kArchiveZIP, "scripts_x2.zip"       , 109);
	indexOptionalArchive(Aurora::kArchiveZIP, "soundsets_x2.zip"     , 110);
	indexOptionalArchive(Aurora::kArchiveZIP, "sounds_x2.zip"        , 111);
	indexOptionalArchive(Aurora::kArchiveZIP, "speedtree_x2.zip"     , 112);
	indexOptionalArchive(Aurora::kArchiveZIP, "templates_x2.zip"     , 113);
	indexOptionalArchive(Aurora::kArchiveZIP, "vo_x2.zip"            , 114);

	progress.step("Loading patch resource files");

	indexOptionalArchive(Aurora::kArchiveZIP, "actors_v103x1.zip"         , 150);
	indexOptionalArchive(Aurora::kArchiveZIP, "actors_v106.zip"           , 151);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_v101.zip"       , 152);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_v107.zip"       , 153);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_v121.zip"       , 154);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_x1_v121.zip"    , 155);
	indexOptionalArchive(Aurora::kArchiveZIP, "lod-merged_x2_v121.zip"    , 156);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v103x1.zip" , 157);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v104.zip"   , 158);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v106.zip"   , 159);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v107.zip"   , 160);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v110.zip"   , 161);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v112.zip"   , 162);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_v121.zip"   , 163);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_x1_v113.zip", 164);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_materials_x1_v121.zip", 165);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v103x1.zip"    , 166);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v104.zip"      , 167);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v105.zip"      , 168);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v106.zip"      , 169);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v107.zip"      , 160);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v112.zip"      , 171);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_v121.zip"      , 172);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_x1_v121.zip"   , 173);
	indexOptionalArchive(Aurora::kArchiveZIP, "nwn2_models_x2_v121.zip"   , 174);
	indexOptionalArchive(Aurora::kArchiveZIP, "templates_v112.zip"        , 175);
	indexOptionalArchive(Aurora::kArchiveZIP, "templates_v122.zip"        , 176);
	indexOptionalArchive(Aurora::kArchiveZIP, "templates_x1_v122.zip"     , 177);
	indexOptionalArchive(Aurora::kArchiveZIP, "vo_103x1.zip"              , 178);
	indexOptionalArchive(Aurora::kArchiveZIP, "vo_106.zip"                , 179);

	progress.step("Indexing extra sound resources");
	indexMandatoryDirectory("ambient"   , 0,  0, 200);
	indexOptionalDirectory ("ambient_x1", 0,  0, 201);
	indexOptionalDirectory ("ambient_x2", 0,  0, 202);
	progress.step("Indexing extra music resources");
	indexMandatoryDirectory("music"     , 0,  0, 203);
	indexOptionalDirectory ("music_x1"  , 0,  0, 204);
	indexOptionalDirectory ("music_x2"  , 0,  0, 205);
	progress.step("Indexing extra movie resources");
	indexMandatoryDirectory("movies"    , 0,  0, 206);
	progress.step("Indexing extra effects resources");
	indexMandatoryDirectory("effects"   , 0,  0, 207);
	progress.step("Indexing extra character resources");
	indexMandatoryDirectory("localvault", 0,  0, 208);
	progress.step("Indexing extra UI resources");
	indexMandatoryDirectory("ui"        , 0, -1, 209);

	progress.step("Indexing Windows-specific resources");
	indexMandatoryArchive(Aurora::kArchiveEXE, "nwn2main.exe", 250);

	progress.step("Indexing override files");
	indexOptionalDirectory("override", 0, 0, 500);

	_hasXP1 = ResMan.hasArchive(Aurora::kArchiveZIP, "2da_x1.zip");
	_hasXP2 = ResMan.hasArchive(Aurora::kArchiveZIP, "2da_x2.zip");
	_hasXP3 = ResMan.hasArchive(Aurora::kArchiveERF, "westgate.hak");

	progress.step("Loading main talk table");
	TalkMan.addTable("dialog", "dialogf", false, 0);

	progress.step("Registering file formats");
	registerModelLoader(new NWN2ModelLoader);
	FontMan.setFormat(Graphics::Aurora::kFontFormatTTF);
}

void NWN2Engine::initCursors() {
	CursorMan.add("cursor0" , "default"  , "up"  );
	CursorMan.add("cursor1" , "default"  , "down");

	CursorMan.setDefault("default", "up");
}

void NWN2Engine::initConfig() {
	// Enable/Disable the Proof-of-Concept software tinting
	ConfigMan.setBool(Common::kConfigRealmDefault, "tint", true);
}

void NWN2Engine::initGameConfig() {
	ConfigMan.setBool(Common::kConfigRealmGameTemp, "NWN2_hasXP1", _hasXP1);
	ConfigMan.setBool(Common::kConfigRealmGameTemp, "NWN2_hasXP2", _hasXP2);
	ConfigMan.setBool(Common::kConfigRealmGameTemp, "NWN2_hasXP3", _hasXP3);
}

void NWN2Engine::deinit() {
}

void NWN2Engine::playIntroVideos() {
	playVideo("atarilogo");
	playVideo("oeilogo");
	playVideo("wotclogo");
	playVideo("nvidialogo");
	playVideo("legal");
	playVideo("intro");
}

void NWN2Engine::main() {
	_campaign = new Campaign(*_console);

	const std::list<CampaignDescription> &campaigns = _campaign->getCampaigns();
	if (campaigns.empty())
		error("No campaigns found");

	_campaign->load(*campaigns.begin());
	_campaign->run();
	_campaign->clear();

	delete _campaign;
	_campaign = 0;
}

} // End of namespace NWN2

} // End of namespace Engines
