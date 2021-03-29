#include "user_main.h"
#include "cmsis_os.h"
#include "common.h"
#include "fx.h"
#include "main.h"
#include "ssd1306.hpp"
#include "stm32f7xx_hal_i2s.h"
#include <cmath>
#include <string.h> // memset

#ifdef TUNER_ENABLED
#include "tuner.h"
#endif

extern I2C_HandleTypeDef hi2c1;
extern I2S_HandleTypeDef hi2s2;
extern I2S_HandleTypeDef hi2s3;

// 定数定義
namespace {
/// 位置型
struct Position {
    uint8_t x;
    uint8_t y;
};
/// エフェクト名 ステータス 表示位置
constexpr Position POS_STATUS = { 4, 0 };
/// エフェクトパラメータ ページ 表示位置
constexpr Position POS_PAGE = { 88, 0 };
/// 処理時間% 表示位置
constexpr Position POS_PERCENT = { 107, 0 };
/// カーソル位置
constexpr Position POS_CURSOR[6] = { { 0, 11 }, { 0, 29 }, { 0, 47 }, { 65, 11 }, { 65, 29 }, { 65, 47 } };
/// エフェクトパラメータ名表示位置
constexpr Position POS_PARAM_NAME[6] = { { 0, 17 }, { 0, 35 }, { 0, 53 }, { 65, 17 }, { 65, 35 }, { 65, 53 } };
/// エフェクトパラメータ数値表示 右端の文字位置
constexpr Position POS_PARAM_VALUE[6] = { { 52, 11 }, { 52, 29 }, { 52, 47 }, { 117, 11 }, { 117, 29 }, { 117, 47 } };
/// I2Sの割り込み間隔時間
constexpr float I2S_INTERRUPT_INTERVAL = static_cast<float>(fx::BLOCK_SIZE) / SAMPLING_FREQ;
/// スイッチ短押しのカウント数（1つのスイッチは4回に1回の読取のため4をかける）
constexpr uint32_t SHORT_PUSH_COUNT = 1 + SHORT_PUSH_MSEC / (4 * 1000 * I2S_INTERRUPT_INTERVAL);
/// スイッチ長押しのカウント数（1つのスイッチは4回に1回の読取のため4をかける）
constexpr uint32_t LONG_PUSH_COUNT = 1 + LONG_PUSH_MSEC / (4 * 1000 * I2S_INTERRUPT_INTERVAL);
/// ステータス情報表示時間のカウント数（1つのスイッチは4回に1回の読取のため4をかける）
constexpr uint32_t STATUS_DISP_COUNT = 1 + STATUS_DISP_MSEC / (1000 * I2S_INTERRUPT_INTERVAL);
} // namespace

// スタティック変数定義
namespace {
/// 音声信号受信バッファ配列 Lch前半 Lch後半 Rch前半 Rch後半
int32_t s_rxBuffer[fx::BLOCK_SIZE * 4] = {};
/// 音声信号送信バッファ配列
int32_t s_txBuffer[fx::BLOCK_SIZE * 4] = {};
/// I2Sの割り込みごとにカウントアップ タイマとして利用
uint32_t s_callbackCount = 0;
/// CPU使用サイクル数 各エフェクトごとに最大値を記録
uint32_t s_cpuUsageCycleMax[fx::COUNT] = {};
/// エフェクトパラメータ 現在何番目か ※0から始まる
uint8_t s_fxParamIdx = 0;
/// エフェクト種類変更フラグ 次エフェクトへ: 1 前エフェクトへ: -1
int s_fxChangeFlag = 0;
/// パラメータ選択カーソル位置 0 ～ 5
uint8_t s_cursorPosition = 0;
/// ステータス表示文字列
char const* s_statusStr = PEDAL_NAME;
/// 動作モード定義
enum MODE { NORMAL, TAP, TUNER };
/// 動作モード 0:通常 1:タップテンポ 2:チューナー
MODE s_currentMode = NORMAL;
} // namespace

/// 現在のエフェクトパラメータ
FxParam g_fxParam[PARAM_COUNT];
/// 現在のエフェクト番号
uint8_t g_fxNum = 0;
/// 全てのエフェクトパラメータデータ配列
int16_t g_fxAllData[fx::COUNT][PARAM_COUNT] = {};
/// タップテンポ入力時間 ms
float g_tapTime = 0.0f;

namespace {
/// @brief エフェクト切替時、データ保存時のミュート
inline void mute() { memset(s_txBuffer, 0, sizeof(s_txBuffer)); }
/// @brief エフェクト変更
inline void fxChange() {
    mute();
    fx::deinit();
    fx::change(s_fxChangeFlag);
    s_fxParamIdx = 0;
    s_cursorPosition = 0;
    fx::init();
    s_fxChangeFlag = 0;
}
/// @brief データ読み込み
inline void loadData() {
    uint32_t addr = DATA_ADDR;
    for (uint32_t i = 0; i < fx::COUNT; i++) // エフェクトデータ フラッシュ読込
    {
        for (uint32_t j = 0; j < PARAM_COUNT; j++) {
            g_fxAllData[i][j] = *reinterpret_cast<uint16_t*>(addr);
            addr += 2;
        }
    }
    g_fxNum = *reinterpret_cast<uint16_t*>(addr);
    if (g_fxNum >= fx::COUNT) {
        g_fxNum = 0;
    }
}
/// @brief データ全消去
inline void eraseData() {
    HAL_FLASH_Unlock(); // フラッシュ ロック解除
    FLASH_EraseInitTypeDef erase = {};
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = DATA_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t error = 0;
    HAL_FLASHEx_Erase(&erase, &error); // フラッシュ消去
    HAL_FLASH_Lock();                  // フラッシュ ロック
}
/// @brief データ保存
inline void saveData() {
    mute();
    ssd1306_xyWriteStrWT(POS_STATUS.x, POS_STATUS.y, "WRITING... ", Font_7x10);

    eraseData(); // フラッシュ消去

    HAL_FLASH_Unlock(); // フラッシュ ロック解除

    // 現在のパラメータをデータ配列へ移す
    for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
        g_fxAllData[g_fxNum][i] = g_fxParam[i].value;
    }

    uint32_t addr = DATA_ADDR;
    for (uint32_t i = 0; i < fx::COUNT; i++) // フラッシュ書込
    {
        for (uint32_t j = 0; j < PARAM_COUNT; j++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, g_fxAllData[i][j]);
            addr += 2;
        }
    }
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, g_fxNum); // 保存時のエフェクト番号記録

    HAL_FLASH_Lock(); // フラッシュ ロック

    s_statusStr = "STORED!    "; // ステータス表示
    s_callbackCount = 0;

    DWT->CYCCNT = 0; // CPU使用率計算に影響しないように、CPUサイクル数を一旦リセット
}
/// @brief スイッチ処理
/// @param[in] num
inline void swProcess(uint8_t num) {
    static uint32_t swCount[10] = {}; // スイッチが押されている間カウントアップ

    switch (num) {
    case 0: // 左上スイッチ --------------------------------------------------------
        if (!LL_GPIO_IsInputPinSet(SW0_UPPER_L_GPIO_Port, SW0_UPPER_L_Pin)) {
            swCount[num]++;
            // 長押し 1回のみ動作
            if (swCount[num] == LONG_PUSH_COUNT) {
                // 左下スイッチが押されている場合
                if (swCount[num + 1] > SHORT_PUSH_COUNT) {
                    swCount[num + 1] = LONG_PUSH_COUNT; // 左下スイッチも長押し済み扱いにする
                    saveData();                         // データ保存
                }
                else {
                    s_fxChangeFlag = -1;
                }
            }
        }
        else {
            // 短押し 離した時の処理
            if (swCount[num] >= SHORT_PUSH_COUNT && swCount[num] < LONG_PUSH_COUNT) {
                // 右上スイッチが押されている場合、パラメータ数値を最大値へ
                if (swCount[num + 2] > SHORT_PUSH_COUNT) {
                    swCount[num + 2] = LONG_PUSH_COUNT + 1; // 右上スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = g_fxParam[s_fxParamIdx].max;
                }
                // エフェクトパラメータ選択位置変更 0→最大値で循環
                else {
                    s_fxParamIdx = (fx::getParamTypeCount() + s_fxParamIdx - 1) % fx::getParamTypeCount();
                    s_cursorPosition = s_fxParamIdx % 6;
                }
            }
            swCount[num] = 0;
        }
        break;
    case 1: // 左下スイッチ --------------------------------------------------------
        if (!LL_GPIO_IsInputPinSet(SW1_LOWER_L_GPIO_Port, SW1_LOWER_L_Pin)) {
            swCount[num]++;
            // 長押し 1回のみ動作
            if (swCount[num] == LONG_PUSH_COUNT) {
                // 左上スイッチが押されている場合
                if (swCount[num - 1] > SHORT_PUSH_COUNT) {
                    swCount[num - 1] = LONG_PUSH_COUNT; // 左上スイッチも長押し済み扱いにする
                    saveData();                         // データ保存
                }
                else {
                    s_fxChangeFlag = 1;
                }
            }
        }
        else {
            // 短押し 離した時の処理
            if (swCount[num] >= SHORT_PUSH_COUNT && swCount[num] < LONG_PUSH_COUNT) {
                // 右下スイッチが押されている場合、パラメータ数値を最小値へ
                if (swCount[num + 2] > SHORT_PUSH_COUNT) {
                    swCount[num + 2] = LONG_PUSH_COUNT + 1; // 右下スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = g_fxParam[s_fxParamIdx].min;
                }
                // エフェクトパラメータ選択位置変更 最大値→0で循環
                else {
                    s_fxParamIdx = (s_fxParamIdx + 1) % fx::getParamTypeCount();
                    s_cursorPosition = s_fxParamIdx % 6;
                }
            }
            swCount[num] = 0;
        }
        break;
    case 2: // 右上スイッチ --------------------------------------------------------
        if (!LL_GPIO_IsInputPinSet(SW2_UPPER_R_GPIO_Port, SW2_UPPER_R_Pin)) {
            swCount[num]++;
            if (swCount[num] >= LONG_PUSH_COUNT / 2 &&
                (swCount[num] % (LONG_PUSH_COUNT / 4)) == 0) // 長押し 繰り返し動作
            {
                g_fxParam[s_fxParamIdx].value =
                    std::min<int32_t>(g_fxParam[s_fxParamIdx].value + 10, g_fxParam[s_fxParamIdx].max);
            }
        }
        else {
            // 短押し 離した時の処理
            if (swCount[num] >= SHORT_PUSH_COUNT && swCount[num] < LONG_PUSH_COUNT) {
                // 右下スイッチが押されている場合、パラメータ数値を中間値へ
                if (swCount[num + 1] > SHORT_PUSH_COUNT) {
                    swCount[num + 1] = LONG_PUSH_COUNT + 1; // 右下スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = (g_fxParam[s_fxParamIdx].min + g_fxParam[s_fxParamIdx].max) / 2;
                }
                // 左上スイッチが押されている場合、パラメータ数値を最大値へ
                else if (swCount[num - 2] > SHORT_PUSH_COUNT) {
                    swCount[num - 2] = LONG_PUSH_COUNT + 1; // 左上スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = g_fxParam[s_fxParamIdx].max;
                }
                else
                    g_fxParam[s_fxParamIdx].value =
                        std::min<int32_t>(g_fxParam[s_fxParamIdx].value + 1, g_fxParam[s_fxParamIdx].max);
            }
            swCount[num] = 0;
        }
        break;
    case 3: // 右下スイッチ --------------------------------------------------------
        if (!LL_GPIO_IsInputPinSet(SW3_LOWER_R_GPIO_Port, SW3_LOWER_R_Pin)) {
            swCount[num]++;
            if (swCount[num] >= LONG_PUSH_COUNT / 2 &&
                (swCount[num] % (LONG_PUSH_COUNT / 4)) == 0) // 長押し 繰り返し動作
            {
                {
                    g_fxParam[s_fxParamIdx].value =
                        std::max<int32_t>(g_fxParam[s_fxParamIdx].value - 10, g_fxParam[s_fxParamIdx].min);
                }
            }
        }
        else {
            // 短押し 離した時の処理
            if (swCount[num] >= SHORT_PUSH_COUNT && swCount[num] < LONG_PUSH_COUNT) {
                // 右上スイッチが押されている場合、パラメータ数値を中間値へ
                if (swCount[num - 1] > SHORT_PUSH_COUNT) {
                    swCount[num - 1] = LONG_PUSH_COUNT + 1; // 右上スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = (g_fxParam[s_fxParamIdx].min + g_fxParam[s_fxParamIdx].max) / 2;
                }
                // 左下スイッチが押されている場合、パラメータ数値を最小値へ
                else if (swCount[num - 2] > SHORT_PUSH_COUNT) {
                    swCount[num - 2] = LONG_PUSH_COUNT + 1; // 左下スイッチは長押し済み扱いにする
                    g_fxParam[s_fxParamIdx].value = g_fxParam[s_fxParamIdx].min;
                }
                else
                    g_fxParam[s_fxParamIdx].value =
                        std::max<int32_t>(g_fxParam[s_fxParamIdx].value - 1, g_fxParam[s_fxParamIdx].min);
            }
            swCount[num] = 0;
        }
        break;
    default:
        break;
    }
}
/// @brief フットスイッチ処理
inline void footSwProcess() {
    static uint32_t footSwCount = 0; // スイッチが押されている間カウントアップ
    static float tmpTapTime = 0;     // タップ間隔時間 一時保存用

    if (!LL_GPIO_IsInputPinSet(SW4_FOOT_GPIO_Port, SW4_FOOT_Pin)) {
        footSwCount++;
        // スイッチを押した時のタップ間隔時間を記録
        if (s_currentMode == TAP && footSwCount == 4 * SHORT_PUSH_COUNT) {
            tmpTapTime = (float)s_callbackCount * I2S_INTERRUPT_INTERVAL * 1000.0f;
            s_callbackCount = 0;
        }
        // 長押し
        if (footSwCount == 4 * LONG_PUSH_COUNT) {
            if (s_currentMode == NORMAL) {
#ifdef TAP_ENABLED
                s_currentMode = TAP; // タップテンポモードへ
                s_callbackCount = 0;
#endif
            }
            else {
                s_currentMode = NORMAL; // タップテンポモード、チューナーモード終了
                g_tapTime = 0.0f;       // タップ時間リセット
            }
        }
        if (footSwCount == 12 * LONG_PUSH_COUNT) // 3倍長押し
        {
            if (s_currentMode == TAP) {
#ifdef TUNER_ENABLED
                mute();
                s_currentMode = TUNER; // チューナーモードへ
#endif
            }
        }
    }
    else {
        // 短押し 離した時の処理
        if (footSwCount >= 4 * SHORT_PUSH_COUNT && footSwCount < 4 * LONG_PUSH_COUNT) {
            if (s_currentMode == NORMAL) {
                fx::toggle();
            }
            else if (s_currentMode == TAP) {
                // スイッチを押した時記録していたタップ間隔時間をスイッチを離した時に反映させる
                if (100.0f < tmpTapTime && tmpTapTime < MAX_TAP_TIME) {
                    g_tapTime = tmpTapTime;
                }
                else {
                    g_tapTime = 0.0f;
                }
            }
        }
        footSwCount = 0;
    }
}
/// @brief DMA用に上位16ビットと下位16ビットを入れ替える
/// 負の値の場合に備えて右シフトの場合0埋めする
inline int32_t swap16(int32_t x) { return (0x0000FFFF & x >> 16) | x << 16; }
/// @brief メイン信号処理等
/// @param[in] start_sample
inline void mainProcess(uint16_t start_sample) {
    DWT->CYCCNT = 0; // CPU使用率計算用 CPUサイクル数をリセット

    float xL[fx::BLOCK_SIZE] = {}; // Lch float計算用データ
    float xR[fx::BLOCK_SIZE] = {}; // Rch float計算用データ 不使用

    for (uint32_t i = 0; i < fx::BLOCK_SIZE; i++) {
        // データ配列の偶数添字計算 Lch（Rch不使用）
        uint16_t m = (start_sample + i) * 2;
        // 受信データを計算用データ配列へ 値を-1～+1(float)へ変更
        xL[i] = static_cast<float>(swap16(s_rxBuffer[m])) / 2147483648.0f;
    }

    if (s_currentMode == TUNER) {
#ifdef TUNER_ENABLED
        tunerProcess(xL, xR); // チューナー
#endif
    }
    else {
        fx::process(xL, xR); // エフェクト処理 計算用配列を渡す
    }

    for (uint32_t i = 0; i < fx::BLOCK_SIZE; i++) {
        // オーバーフロー防止
        if (xL[i] < -1.0f) {
            xL[i] = -1.0f;
        }
        if (xL[i] > 0.99f) {
            xL[i] = 0.99f;
        }
        uint16_t m = (start_sample + i) * 2; // データ配列の偶数添字計算 Lch（Rch不使用）
        // 計算済データを送信バッファへ 値を32ビット整数へ戻す
        s_txBuffer[m] = swap16((int32_t)(2147483648.0f * xL[i]));
    }

    s_callbackCount++; // I2Sの割り込みごとにカウントアップ タイマとして利用
    footSwProcess();   // フットスイッチ処理
    if (s_currentMode == NORMAL) {
        swProcess(s_callbackCount % 4); // 割り込みごとにスイッチ処理するが、スイッチ1つずつを順番に行う
        const uint32_t cyccnt = DWT->CYCCNT;
        s_cpuUsageCycleMax[g_fxNum] = std::max<uint32_t>(s_cpuUsageCycleMax[g_fxNum], cyccnt); // CPU使用率計算用
    }
    // ※ディレイメモリ確保前に信号処理に進まないように割り込み内で行う
    if (s_fxChangeFlag) {
        fxChange(); // エフェクト変更
    }
}
/// @brief エフェクト画面表示
inline void fxDisp() {
    uint8_t fxPage = s_fxParamIdx / 6; // エフェクトパラメータページ番号
    // ステータス表示------------------------------
    if (s_callbackCount > STATUS_DISP_COUNT) // ステータス表示が変わり一定時間経過後、デフォルト表示に戻す
    {
        s_statusStr = fx::getName(); // エフェクト名表示
    }
    ssd1306_xyWriteStrWT(POS_STATUS.x, POS_STATUS.y, s_statusStr, Font_7x10);
    // エフェクトパラメータ名称表示------------------------------
    for (int i = 0; i < 6; i++) {
        ssd1306_xyWriteStrWT(POS_PARAM_NAME[i].x, POS_PARAM_NAME[i].y, g_fxParam[i + 3 * fxPage].nameTxt, Font_7x10);
    }
    // エフェクトパラメータ数値表示------------------------------
    for (int i = 0; i < 6; i++) {
        fx::setParamStr(i + 6 * fxPage); // パラメータ数値を文字列に変換
        ssd1306_R_xyWriteStrWT(
            POS_PARAM_VALUE[i].x, POS_PARAM_VALUE[i].y, g_fxParam[i + 3 * fxPage].valueTxt, Font_11x18);
    }
    // エフェクトパラメータページ番号表示------------------------------
    {
        char str[8] = { 0 };
        snprintf(str, sizeof(str), "P%d", fxPage + 1);
        ssd1306_xyWriteStrWT(POS_PAGE.x, POS_PAGE.y, str, Font_7x10);
    }
    // カーソル表示(選択したパラメータの白黒反転) ------------------------------
    for (int dx = 0; dx < 62; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            ssd1306_InvertPixel(POS_CURSOR[s_cursorPosition].x + dx, POS_CURSOR[s_cursorPosition].y + dy);
        }
    }
    // CPU使用率表示------------------------------
    {
        auto cpuUsagePercent = 100.0f * s_cpuUsageCycleMax[g_fxNum] / SystemCoreClock / I2S_INTERRUPT_INTERVAL;
        char str[8] = { 0 };
        snprintf(str, sizeof(str), "%2d%%", static_cast<int>(cpuUsagePercent));
        ssd1306_xyWriteStrWT(POS_PERCENT.x, POS_PERCENT.y, str, Font_7x10);
    }
}
/// @brief TAP画面表示
inline void tapDisp() {
    ssd1306_xyWriteStrWT(0, 0, "TAP TEMPO", Font_7x10);
    std::string tmpStr = std::to_string((uint16_t)g_tapTime);
    ssd1306_R_xyWriteStrWT(114, 0, tmpStr + " ms", Font_7x10); // タップ間隔時間を表示
    if (g_tapTime > 60.0f) {
        tmpStr = std::to_string((uint16_t)(60000.0f / g_tapTime)); // bpmを計算
    }
    ssd1306_R_xyWriteStrWT(103, 20, tmpStr + " bpm", Font_16x26); // bpm表示

    uint16_t blinkCount = 1 + g_tapTime / (1000 * I2S_INTERRUPT_INTERVAL);   // 点滅用カウント数
    if (s_callbackCount % blinkCount < 60 / (1000 * I2S_INTERRUPT_INTERVAL)) // 60msバーを表示、点滅
    {
        for (int i = 0; i < 112; i++) {
            ssd1306_DrawPixel(8 + i, 47, White);
            ssd1306_DrawPixel(8 + i, 48, White);
        }
    }
}
/// @brief LED表示
inline void ledDisp() {
    // LED表示------------------------------
    // RGB565を変換 PWMで色を制御する場合使えるかも
    const uint16_t led = fx::getLedColor();
    const uint8_t r = static_cast<uint8_t>((led >> 8) & 0b0000000011111000);
    const uint8_t g = static_cast<uint8_t>((led >> 3) & 0b0000000011111100);
    const uint8_t b = static_cast<uint8_t>((led << 3) & 0b0000000011111000);
    if (!!r) {
        LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
    if (!!g) {
        LL_GPIO_SetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
    if (!!b) {
        LL_GPIO_SetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
    }
    else {
        LL_GPIO_ResetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
    }
}
} // namespace

/// @brief 最初に1回のみ行う処理
void mainInit() {
    // Denormalized numbers（非正規化数）を0として扱うためFPSCRレジスタ変更
    asm("VMRS r0, FPSCR");
    asm("ORR r0, r0, #(1 << 24)");
    asm("VMSR FPSCR, r0");

    // 処理時間計測用設定
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // ディスプレイ初期化
    ssd1306_Init(&hi2c1);

#if 0
    // ディスプレイ点灯確認
    ssd1306_Fill(White);
    ssd1306_SetCursor(3, 22);
    ssd1306_WriteString(PEDAL_NAME, Font_11x18, Black);
    ssd1306_UpdateScreen(&hi2c1);

    // LED点灯確認
    LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    osDelay(300);
    LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    LL_GPIO_SetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    osDelay(300);
    LL_GPIO_ResetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    LL_GPIO_SetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
    osDelay(300);
    LL_GPIO_ResetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
#endif

    // I2SのDMA開始
    HAL_I2S_Transmit_DMA(&hi2s2, reinterpret_cast<uint16_t*>(s_txBuffer), fx::BLOCK_SIZE * 4);
    HAL_I2S_Receive_DMA(&hi2s3, reinterpret_cast<uint16_t*>(s_rxBuffer), fx::BLOCK_SIZE * 4);

    // オーディオコーデック リセット
    LL_GPIO_ResetOutputPin(CODEC_RST_GPIO_Port, CODEC_RST_Pin);
    osDelay(100);
    LL_GPIO_SetOutputPin(CODEC_RST_GPIO_Port, CODEC_RST_Pin);
    osDelay(100);

    // I2Sのフレームエラー発生の場合、リセットを繰り返す
    while (__HAL_I2S_GET_FLAG(&hi2s2, I2S_FLAG_FRE) || __HAL_I2S_GET_FLAG(&hi2s3, I2S_FLAG_FRE)) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("ERROR", Font_11x18, Black);
        ssd1306_UpdateScreen(&hi2c1);
        LL_GPIO_ResetOutputPin(CODEC_RST_GPIO_Port, CODEC_RST_Pin);
        osDelay(100);
        LL_GPIO_SetOutputPin(CODEC_RST_GPIO_Port, CODEC_RST_Pin);
        osDelay(100);
    }

    // 起動時フットスイッチ、左上スイッチ、右下スイッチを押していた場合、データ全消去
    if (!LL_GPIO_IsInputPinSet(SW0_UPPER_L_GPIO_Port, SW0_UPPER_L_Pin) &&
        !LL_GPIO_IsInputPinSet(SW3_LOWER_R_GPIO_Port, SW3_LOWER_R_Pin) &&
        !LL_GPIO_IsInputPinSet(SW4_FOOT_GPIO_Port, SW4_FOOT_Pin)) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("ERASE ALL DATA", Font_7x10, Black);
        ssd1306_UpdateScreen(&hi2c1);
        eraseData();
        osDelay(1000);
    }

    // 保存済パラメータ読込
    loadData();

    // 初期エフェクト読込
    fx::init();
}

/// @brief メインループ
void mainLoop() {

    ssd1306_Fill(Black); // 一旦画面表示を全て消す
    switch (s_currentMode) {
    case NORMAL:
        fxDisp();
        break;
    case TAP:
        tapDisp();
        break;
#ifdef TUNER_ENABLED
    case TUNER:
        tunerDisp();
        break;
#endif
    }
    ssd1306_UpdateScreen(&hi2c1); // 画面更新
    // osDelay(10);
    ledDisp();
}

/// @brief I2Sの受信バッファに半分データがたまったときの割り込み
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    mainProcess(0); // 0 ～ 15 を処理(0 ～ BLOCK_SIZE-1)
}

/// @brief I2Sの受信バッファに全データがたまったときの割り込み
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef* hi2s) {
    mainProcess(fx::BLOCK_SIZE); // 16 ～ 31 を処理(BLOCK_SIZE ～ BLOCK_SIZE*2-1)
}
