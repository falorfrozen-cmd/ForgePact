#pragma once
#include <ForgePact/MinerHelmetModel.hpp>
namespace ForgePact::MinerHelmet {
inline bool pending = false, enabled = false, worn = false, equipmentReadable = false;
inline bool hudNative = false;
inline unsigned rewards = 0, wavesStarted = 0, bonusVeins = 0;
inline std::string grantRequest, grantResult = "Not requested";
inline std::string lastRewardReason = "No mining ore reward observed";
inline unsigned rewardRefusalsLogged = 0;
inline void Arm();
inline void Tick();
inline void Draw();
inline void Command(const std::string& args);
}
