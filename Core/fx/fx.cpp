#include "fx.hpp"
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
fx::base* s_effects[FX_COUNT] = { &s_od1, &s_dd1, &s_tr1, &s_ce1, &s_ph1, &s_rv1 };
/// エフェクトオン・オフ
bool s_on = false;
} // namespace

char const* fx::getName() { return s_effects[g_fxNum]->getFxName(); }

uint16_t fx::getLedColor() { return s_effects[g_fxNum]->getLedColor(s_on); }

uint8_t fx::getParamTypeCount() { return s_effects[g_fxNum]->getParamTypeCount(); }

void fx::process(float xL[], float xR[]) { s_effects[g_fxNum]->process(xL, xR, s_on); }

void fx::init() { s_effects[g_fxNum]->init(g_fxAllData[g_fxNum]); }

void fx::deinit() { s_effects[g_fxNum]->deinit(); }

void fx::setParamStr(uint8_t paramNum) { s_effects[g_fxNum]->setParamStr(paramNum); }

void fx::change(int shiftCount) { g_fxNum = (FX_COUNT + g_fxNum + shiftCount) % FX_COUNT; }

void fx::toggle() { s_on = !s_on; }
