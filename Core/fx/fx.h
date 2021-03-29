#pragma once

#include "fx_base.h"

namespace fx {
/// エフェクト総数
constexpr uint32_t COUNT = 6;
/// @brief エフェクト名文字列 取得
/// @return エフェクト名文字列
char const* getName();
/// @brief LED色(RGB565) 取得
/// @return LED色(RGB565)
uint16_t getLedColor();
/// @brief パラメータ総数 取得
/// @return パラメータ総数
uint8_t getParamTypeCount();
/// @brief エフェクト処理
void process(float (&xL)[BLOCK_SIZE], float (&xR)[BLOCK_SIZE]);
/// @brief 初期化処理 パラメータ読込、ディレイ用メモリ確保等
void init();
/// @brief 終了処理 ディレイ用メモリ縮小等
void deinit();
/// @brief エフェクトパラメータ文字列更新処理
void setParamStr(uint8_t paramIdx);
/// @brief エフェクト種類切替
/// @param shiftCount 切替方向
void change(int shiftCount);
/// @brief エフェクトオン・オフ切替
void toggle();
} // namespace fx
