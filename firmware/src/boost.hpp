#pragma once
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include <cassert>

#include "pwm.hpp"
#include "types.hpp"

namespace boost {
using namespace types;

struct Config {
    u8 en;
    u8 sw;
    float max_duty;
    pwm::Config pwm;
    inline constexpr Config(const u8 en_, const u8 sw_, const u8 pwm_slice,
                            const pwm::Channel pwm_channel, const u32 freq,
                            const float max_duty_)
        : pwm(sw_, pwm_slice, pwm_channel, false, freq) {
        en = en_;
        sw = sw_;
        max_duty = max_duty_;
    }
};

template <const Config CONF>
class Boost {
   private:
    pwm::Pwm<CONF.pwm> pwm;

   public:
    inline constexpr Boost() { pwm = pwm::Pwm<CONF.pwm>(); }
    // requires pwm to be initialised
    inline void init(const bool start) {
        this->pwm.init(start);
        gpio_disable_pulls(CONF.en);
        gpio_set_function(CONF.en, GPIO_FUNC_SIO);
        gpio_put(CONF.en, 1);
        gpio_set_dir(CONF.en, true);
    }
    /// 0 to `CONF.max_duty`
    inline void set(float duty) {
        if (duty > CONF.max_duty) {
            duty = CONF.max_duty;
        }
        this->pwm.set_duty(duty);
    }
};

}  // namespace boost
