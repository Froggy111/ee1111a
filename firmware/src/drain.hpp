#pragma once
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include <cassert>

#include "pwm.hpp"
#include "types.hpp"

namespace drain {
using namespace types;

struct Config {
    u8 en;
    u8 nsw;
    pwm::Config pwm;
    inline constexpr Config(const u8 en_, const u8 nsw_, const u8 pwm_slice,
                            const pwm::Channel pwm_channel, const u32 freq)
        : pwm(nsw_, pwm_slice, pwm_channel, true, freq) {
        en = en_;
        nsw = nsw_;
    }
};

template <const Config CONF>
class Drain {
   private:
    pwm::Pwm<CONF.pwm> pwm;

   public:
    inline constexpr Drain() { pwm = pwm::Pwm<CONF.pwm>(); }
    // requires pwm to be initialised
    inline void init(const bool start) {
        this->pwm.init(start);
        gpio_disable_pulls(CONF.en);
        gpio_set_function(CONF.en, GPIO_FUNC_SIO);
        gpio_put(CONF.en, 1);
        gpio_set_dir(CONF.en, true);
    }
    // 0 to 1
    inline void set(const float duty) { this->pwm.set_duty(duty); }
};

}  // namespace drain
