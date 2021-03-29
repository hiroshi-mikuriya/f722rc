#pragma once

#include "fx.h"
#include <cstdint>
#include <string>

/// タップテンポ機能
#define TAP_ENABLED
/// チューナー機能
#define TUNER_ENABLED

/* 各定数設定 --------------------------*/

/// ペダル名称表示
constexpr char const* PEDAL_NAME = "Sodium v0.7";

/// サンプリング周波数
constexpr float SAMPLING_FREQ = 44108.07f;

/// スイッチ短押し時間 ミリ秒
constexpr uint32_t SHORT_PUSH_MSEC = 20;

/// スイッチ長押し時間 ミリ秒
constexpr uint32_t LONG_PUSH_MSEC = 1000;

/// ステータス情報表示時間 ミリ秒
constexpr uint32_t STATUS_DISP_MSEC = 1000;

/// 各エフェクトのパラメータ数
constexpr uint32_t PARAM_COUNT = 20;

/// タップテンポ最大時間 ミリ秒
constexpr float MAX_TAP_TIME = 3000.0f;

/// データ保存先 セクターと開始アドレス
#define DATA_SECTOR FLASH_SECTOR_5
constexpr uint32_t DATA_ADDR = 0x08020000;

/* 関数 -------------------------------------*/

/// 円周率
constexpr float PI = 3.14159265359f;

/// 最小値a、最大値bでクリップ
#define clip(x, a, b) (((x) < (a) ? (a) : (x)) > (b) ? (b) : ((x) < (a) ? (a) : (x)))

/* グローバル変数 --------------------------*/

/// エフェクトパラメータ型
struct FxParam {
    int16_t value = 1;        ///< パラメータ値
    int16_t max = 1;          ///< パラメータ最大値
    int16_t min = 0;          ///< パラメータ最小値
    char const* nameTxt = 0;  ///< パラメータ名文字列
    char valueTxt[8] = { 0 }; ///< パラメータ値文字列
};

// user_main.cpp で定義
extern FxParam g_fxParam[PARAM_COUNT];
extern uint8_t g_fxNum;
extern int16_t g_fxAllData[fx::COUNT][PARAM_COUNT];
extern float g_tapTime;
