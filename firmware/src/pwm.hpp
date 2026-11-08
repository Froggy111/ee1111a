#pragma once

#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include <cassert>

#include "types.hpp"

namespace pwm {

using namespace types;

struct CalcRes {
    u8 div;
    u8 div_frac;
    u16 reload;
};

const u32 PWM_CLK = 125e6;

inline constexpr CalcRes pwm_calc(const u32 freq) {
    assert(freq > 0);
    // cycles per period = reload + 1
    float max_res_freq = (float)PWM_CLK / (float)(0xFFFF + 1);
    if ((float)freq >= max_res_freq) {
        return CalcRes{
            .div = 1, .div_frac = 0, .reload = (u16)(PWM_CLK / freq)};
    } else {
        float min_div = (float)max_res_freq / (float)freq;
        min_div *= (1 << 4);
        i32 int_part = (i32)min_div;
        float frac = min_div - int_part;
        if (frac > 0.0f) {
            min_div = int_part + 1.0f;
        } else {
            min_div = int_part;
        }
        CalcRes calc_res;
        u16 div = (u16)min_div;
        calc_res.div = (u8)(div >> 4);
        calc_res.div_frac = (u8)(div & 0xF);
        min_div /= (1 << 4);
        // cycles per period = reload + 1
        calc_res.reload = (u16)((float)PWM_CLK / ((float)freq * min_div)) + 1;
        assert((div >> 12) == 0);
        assert(calc_res.div >= 1);
        if (calc_res.div == 1) {
            assert(calc_res.div_frac > 0);
        }
        return calc_res;
    }
}

enum class Channel : u8 {
    A = 0,
    B = 1,
};

// this is kinda stupid but not using both channels anywhere here
struct Config {
    inline constexpr Config(const u8 pin, const u8 slice, const Channel channel,
                            const bool invert, const u32 freq) {
        const CalcRes calc_res = pwm_calc(freq);

        pin_ = pin;
        slice_ = slice;
        channel_ = channel;
        invert_ = invert;
        div_ = calc_res.div;
        div_frac_ = calc_res.div_frac;
        reload_ = calc_res.reload;
        freq_ = freq;
    }
    inline constexpr u8 pin() const { return this->pin_; }
    inline constexpr u8 slice() const { return this->slice_; }
    inline constexpr Channel channel() const { return this->channel_; }
    inline constexpr bool invert() const { return this->invert_; }
    inline constexpr u8 div() const { return this->div_; }
    inline constexpr u8 div_frac() const { return this->div_frac_; }
    inline constexpr u16 reload() const { return this->reload_; }
    inline constexpr u32 freq() const { return this->freq_; }
    inline pwm_config conf() const {
        pwm_config conf = pwm_get_default_config();
        pwm_config_set_clkdiv_int_frac4(&conf, this->div_, this->div_frac_);
        pwm_config_set_wrap(&conf, this->reload_);
        switch (this->channel_) {
            case Channel::A:
                pwm_config_set_output_polarity(&conf, this->invert_, false);
                break;
            case Channel::B:
                pwm_config_set_output_polarity(&conf, false, this->invert_);
                break;
        }
        return conf;
    }

    u8 pin_;
    u8 slice_;
    Channel channel_;
    bool invert_;
    u8 div_;
    u8 div_frac_;
    u16 reload_;
    u32 freq_;
};

template <const Config CONF>
class Pwm {
   public:
    inline constexpr Pwm() {}
    inline void init(const bool start) {
        pwm_config conf = CONF.conf();
        pwm_init(CONF.slice(), &conf, start);
        gpio_set_function(CONF.pin(), GPIO_FUNC_PWM);
    }
    inline void set_duty(float duty) {
        if (duty < 0.0f) {
            duty = 0.0f;
        } else if (duty > 1.0f) {
            duty = 1.0f;
        }
        u16 cmp = duty * CONF.reload();
        pwm_set_chan_level(CONF.slice(), (u8)CONF.channel(), cmp);
    }
};

}  // namespace pwm
