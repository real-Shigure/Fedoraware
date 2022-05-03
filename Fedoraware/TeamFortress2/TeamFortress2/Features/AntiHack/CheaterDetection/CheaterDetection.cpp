#include "CheaterDetection.h"


bool CheaterDetection::shouldScan(int nIndex, int friendsID, CBaseEntity* pSuspect) {
	if (g_EntityCache.Friends[nIndex] || (g_GlobalInfo.ignoredPlayers.find(friendsID) != g_GlobalInfo.ignoredPlayers.end()) || markedcheaters[friendsID]) { return false; } // dont rescan this player if we know they are cheating, a friend, or ignored
	if (pSuspect->GetDormant()) { return false; } // dont run this player if they are dormant
	if (!pSuspect->IsAlive() || pSuspect->IsAGhost()) { return false; } // dont run this player if they are dead or ghost
	return true;
}

bool CheaterDetection::isSteamNameDifferent(PlayerInfo_t pInfo) {	// this can be falsely triggered by a person being nicknamed without being our steam friend (pending friend) or changing their steam name since joining the game
	if (const char* steam_name = g_SteamInterfaces.Friends015->GetFriendPersonaName(CSteamID(static_cast<uint64>(pInfo.friendsID + 0x0110000100000000))))
	{
		if (strcmp(pInfo.name, steam_name))
			return true;
	}
	return false;
}

bool CheaterDetection::isPitchInvalid(CBaseEntity* pSuspect) {
	Vec3 suspectAngles = pSuspect->GetEyeAngles();
	if (!suspectAngles.IsZero()) {
		if (suspectAngles.x >= 90.f || suspectAngles.x <= -90.f) {
			return true;
		}
	}
	return false;
}

bool CheaterDetection::isTickCountManipulated(int CurrentTickCount) {
	int delta = g_Interfaces.GlobalVars->tickcount - CurrentTickCount; // delta should be 1 however it can be different me thinks (from looking it only gets to about 3 at its worst, maybe this is different with packet loss?)
	if (abs(delta) > 14) { return true; } // lets be honest if their tickcount changes by more than 14 they are probably cheating.
	return false;
}

void CheaterDetection::OnTick() {
	auto pLocal = g_EntityCache.m_pLocal;
	if (!pLocal || !g_Interfaces.Engine->IsConnected()) {
		return;
	}

	for (const auto& pSuspect : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL)) {
		if (!pSuspect) { continue; }
		int index = pSuspect->GetIndex();


		PlayerInfo_t pi;
		if (g_Interfaces.Engine->GetPlayerInfo(index, &pi) && !pi.fakeplayer) {
			int friendsID = pi.friendsID;

			if (index == pLocal->GetIndex() || !shouldScan(index, friendsID, pSuspect)) { continue; }

			if (!UserData[friendsID].detections.steamname) {
				UserData[friendsID].detections.steamname = true; // to prevent false positives and needless rescanning, set this to true after the first scan.
				strikes[friendsID] += isSteamNameDifferent(pi) ? 1 : 0; // add a strike to this player if they are manipulating their in game name.
			}

			if (!UserData[friendsID].detections.invalidpitch) {
				if (isPitchInvalid(pSuspect)) {
					UserData[friendsID].detections.invalidpitch = true;
					strikes[friendsID] += 5; // because this cannot be falsely triggered, anyone detected by it should be marked as a cheater instantly 
				}
			}

			if (!UserData[friendsID].detections.invalidtext) {
				if (illegalchar[index]) {
					UserData[friendsID].detections.invalidtext = true;
					strikes[friendsID] += 5;
					illegalchar[index] = false;
				}
			}

			int currenttickcount = TIME_TO_TICKS(pSuspect->GetSimulationTime());

			if (g_Interfaces.GlobalVars->tickcount) {
				if (isTickCountManipulated(currenttickcount)) {
					g_Interfaces.CVars->ConsoleColorPrintf({ 255, 255, 0, 255 }, tfm::format("[%s] DEVIATION(%i)", pi.name, abs(g_Interfaces.GlobalVars->tickcount - currenttickcount)).c_str());
					strikes[friendsID] += 1;
				}
			}

			markedcheaters[friendsID] = strikes[friendsID] > 4;
		}
	}
}