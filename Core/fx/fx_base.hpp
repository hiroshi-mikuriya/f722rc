#pragma once

namespace fx {
/// @brief 各エフェクトクラスの基底クラス 純粋仮想関数を含む抽象クラス
class base {
public:
    /// @brief エフェクト名文字列 取得
    /// @return エフェクト名文字列
    virtual char const* getFxName() const = 0;
    /// @brief LED色(RGB565) 取得
    /// @param[in] on エフェクトオン・オフ
    /// @return LED色(RGB565)
    virtual uint16_t getLedColor(bool on) const = 0;
    /// @brief パラメータ総数 取得
    /// @return パラメータ総数
    virtual uint8_t getParamTypeCount() const = 0;
    /// @brief 初期化
    /// I2S受信割込み（ハーフ/フル）からエフェクト種類変更時に呼ばれる
    /// @param[in] loadData フラッシュから読み込んだデータ
    virtual void init(int16_t const* loadData) = 0;
    /// @brief 終了処理
    /// I2S受信割込み（ハーフ/フル）からエフェクト種類変更時に呼ばれる
    virtual void deinit() = 0;
    /// @brief パラメータ文字列を設定
    /// メインループから毎回呼ばれる
    /// @param[in] paramIdx パラメータインデックス
    virtual void setParamStr(uint8_t paramIdx) = 0;
    /// @brief パラメータ設定
    /// processから毎回呼ばれる
    virtual void setParam() = 0;
    /// @brief エフェクト処理
    /// I2S受信割込み（ハーフ/フル）から毎回呼ばれる
    /// @param[inout] xL L音声信号
    /// @param[inout] xR R音声信号
    /// @param[in] on エフェクトオン・オフ
    virtual void process(float xL[], float xR[], bool on) = 0;
};
} // namespace fx
