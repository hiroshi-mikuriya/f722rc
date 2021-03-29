#include "fx.h"
#include "common.h"
#include "fx_chorus.hpp"
#include "fx_delay.hpp"
#include "fx_overdrive.hpp"
#include "fx_phaser.hpp"
#include "fx_reverb.hpp"
#include "fx_tremolo.hpp"

namespace {
/// オーバードライブ
fx::overdrive s_od1;
/// ディレイ
fx::delay s_dd1;
/// トレモロ
fx::tremolo s_tr1;
/// コーラス
fx::chorus s_ce1;
/// フェイザー
fx::phaser s_ph1;
/// リバーブ
fx::reverb s_rv1;
/// エフェクター順序
fx::base* s_effects[fx::COUNT] = { &s_od1, &s_dd1, &s_tr1, &s_ce1, &s_ph1, &s_rv1 };
/// エフェクトオン・オフ
bool s_on = false;
/// @brief 現在選択されているエフェクター取得
/// @return 現在選択されているエフェクター
inline fx::base* current() { return s_effects[g_fxNum]; }
} // namespace

char const* fx::getName() { return current()->getFxName(); }

uint16_t fx::getLedColor() { return current()->getLedColor(s_on); }

uint8_t fx::getParamTypeCount() { return current()->getParamTypeCount(); }

void fx::process(float (&xL)[BLOCK_SIZE], float (&xR)[BLOCK_SIZE]) { current()->process(xL, xR, s_on); }

void fx::init() { current()->init(g_fxAllData[g_fxNum]); }

void fx::deinit() { current()->deinit(); }

void fx::setParamStr(uint8_t paramNum) { current()->setParamStr(paramNum); }

void fx::change(int shiftCount) { g_fxNum = (fx::COUNT + g_fxNum + shiftCount) % fx::COUNT; }

void fx::toggle() { s_on = !s_on; }
