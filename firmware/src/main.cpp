#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/pwm.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>

#include <cstdio>

#include "boost.hpp"
#include "conf.hpp"
#include "drain.hpp"
#include "hwdesc.hpp"
#include "ina219b.hpp"
#include "pid.hpp"
#include "pwm.hpp"
#include "types.hpp"

using namespace types;
using namespace hwdesc;

// NOTE: just using float for things because im too lazy to do fixed point

i2c_inst_t* I2C = I2C_INSTANCE(I2C_INST);
constexpr u32 DRAIN_PWM_FREQ = 1000;
drain::Drain<drain::Config(pins::drain::EN1, pins::drain::NSW1, 4,
                           pwm::Channel::B, DRAIN_PWM_FREQ)>
    cell1_drain;
drain::Drain<drain::Config(pins::drain::EN2, pins::drain::NSW2, 5,
                           pwm::Channel::A, DRAIN_PWM_FREQ)>
    cell2_drain;
drain::Drain<drain::Config(pins::drain::EN3, pins::drain::NSW3, 1,
                           pwm::Channel::B, DRAIN_PWM_FREQ)>
    cell3_drain;

constexpr float BOOST_PWM_FREQ = 1.5e5;
constexpr float BOOST_MAX_DUTY = 0.7;
boost::Boost<boost::Config(pins::BOOST_EN, pins::BOOST_SW, 2, pwm::Channel::A,
                           BOOST_PWM_FREQ, BOOST_MAX_DUTY)>
    boost_ctrl;

ina219b::Ina219B<ina219b::Config{
    .addr = hwdesc::INA219B_ADDR,
    .bus_range = ina219b::BusRange::_16V,
    .shunt_range = ina219b::ShuntRange::_80mV,
    .bus_sampling_mode = ina219b::SamplingMode::_128xOS,
    .shunt_sampling_mode = ina219b::SamplingMode::_128xOS,
    .operating_mode = ina219b::OperatingMode::ShuntBusContinuous,
    .resistance = 0.01f,
    // q1.14
    .current_lsb = 1.0 / (float)(1 << 13)}>
    psensor(I2C);

constexpr float SWITCH_BATT_MODE_THRESHOLD = 12.4;
constexpr float CC_CHARGE_CURRENT = 0.1;
constexpr float BATT_STOP_CHARGING_THRESHOLD = 12.6;
constexpr u16 VSENSE_OVERSAMPLE_COUNT = 128;
constexpr float SHUTOFF_VOLTAGE = 10.5;
constexpr float REENABLE_VOLTAGE = 11.1;
constexpr float PAO_STEP_SIZE = 0.001f;

void init();

struct BattVoltages {
    float cell1;
    float cell2;
    float cell3;
    float total;
};

u16 sat_sub(u16 lhs, u16 rhs) {
    if (rhs > lhs) {
        return 0;
    } else {
        return lhs - rhs;
    }
}

BattVoltages read_voltages() {
    u32 cell1_sum = 0;
    u32 cell2_sum = 0;
    u32 cell3_sum = 0;
    u32 total_sum = 0;
    for (u16 i = 0; i < VSENSE_OVERSAMPLE_COUNT; i++) {
        adc_select_input(CELL1_ADC_CH);
        u16 cell1 = adc_read();
        adc_select_input(CELL2_ADC_CH);
        u16 cell2_raw = adc_read();
        adc_select_input(BATT_ADC_CH);
        u16 total = adc_read();
        u16 cell2 = sat_sub(cell2_raw, cell1);
        u16 cell3 = sat_sub(total, cell2_raw);
        cell1_sum += cell1;
        cell2_sum += cell2;
        cell3_sum += cell3;
        total_sum += total;
    }
    float cell1 =
        ((float)cell1_sum / (float)VSENSE_OVERSAMPLE_COUNT) * VSENSE_SCALE;
    float cell2 =
        ((float)cell2_sum / (float)VSENSE_OVERSAMPLE_COUNT) * VSENSE_SCALE;
    float cell3 =
        ((float)cell3_sum / (float)VSENSE_OVERSAMPLE_COUNT) * VSENSE_SCALE;
    float total =
        ((float)total_sum / (float)VSENSE_OVERSAMPLE_COUNT) * VSENSE_SCALE;
    return BattVoltages{cell1, cell2, cell3, total};
}
inline float read_batt_v() {
    adc_select_input(BATT_ADC_CH);
    u16 val = adc_read();
    return (float)val * VSENSE_SCALE;
}

inline void enable_output() { gpio_put(pins::USB_PWRON, 1); }
inline void disable_output() { gpio_put(pins::USB_PWRON, 0); }

enum class State : u8 {
    Stopped,
    Mppt,
    Cc,
};

int main() {
    init();
    // this is stupid, but im too lazy to set things up properly
    State state = State::Stopped;
    pid::Pid<pid::Config{.kp = -0.1f,
                         .ki = 0.0f,
                         .kd = 0.0f,
                         .min = 0.0f,
                         .max = BOOST_MAX_DUTY}>
        current_pid;
    current_pid.set_target(CC_CHARGE_CURRENT);
    float duty = 0.5f;
    BattVoltages voltages = read_voltages();
    while (!psensor.read_status_bus().conv_ready) {
    }
    float current = psensor.read_current_amps();
    // this clears `conv_ready`
    float power = psensor.read_power_watts();
    float prev_power = power;
    bool pao_dir = true;

    while (true) {
        if (read_batt_v() < SHUTOFF_VOLTAGE) {
            disable_output();
        } else if (read_batt_v() > REENABLE_VOLTAGE) {
            enable_output();
        }

        // TODO: add cell balancing
        ina219b::Status status = psensor.read_status_bus();
        // use ina219 conv as a clock because im lazy
        if (status.conv_ready) {
            current = psensor.read_current_amps();
            // this clears `conv_ready`
            prev_power = power;
            power = psensor.read_power_watts();
            voltages = read_voltages();

            if (voltages.total > BATT_STOP_CHARGING_THRESHOLD) {
                printf("battery voltage: %f, stopping...\n", voltages.total);
                printf("cell1: %f, cell2: %f, cell3: %f\n", voltages.cell1,
                       voltages.cell2, voltages.cell3);
                boost_ctrl.set(0.0f);
                state = State::Stopped;
            } else if ((voltages.total > SWITCH_BATT_MODE_THRESHOLD) &
                       (current > CC_CHARGE_CURRENT)) {
                if (state != State::Cc) {
                    current_pid.reset(current);
                    duty = 0.0f;
                    boost_ctrl.set(duty);
                } else {
                    duty += current_pid.update(current);
                    boost_ctrl.set(duty);
                }
                printf(
                    "cc charging, battery voltage: %f, PV voltage: %f, PV "
                    "current: %f, "
                    "charge power: %f, duty: %f\n",
                    voltages.total, (float)status.bus_voltage * 4.0e-3, current,
                    power, duty);
                printf("cell1: %f, cell2: %f, cell3: %f\n", voltages.cell1,
                       voltages.cell2, voltages.cell3);
                state = State::Cc;
            } else {
                bool dir = pao_dir ^ (power < prev_power);
                if (dir) {
                    duty += PAO_STEP_SIZE;
                } else {
                    duty -= PAO_STEP_SIZE;
                }
                if (duty >= BOOST_MAX_DUTY) {
                    duty = BOOST_MAX_DUTY;
                    dir = false;
                } else if (duty <= 0.0f) {
                    duty = 0.0f;
                    dir = true;
                }
                // duty = 0.5f;
                pao_dir = dir;
                boost_ctrl.set(duty);
                printf(
                    "mppt charging, battery voltage: %f, PV voltage: %f, PV "
                    "current: %f, "
                    "charge power: %f, duty: %f\n",
                    voltages.total, (float)status.bus_voltage * 4.0e-3, current,
                    power, duty);
                printf("cell1: %f, cell2: %f, cell3: %f\n", voltages.cell1,
                       voltages.cell2, voltages.cell3);
                state = State::Mppt;
            }
        }
    }
}

inline void init() {
    stdio_init_all();

    i2c_init(I2C, conf::I2C_BAUD);
    // we have external pullups
    gpio_disable_pulls(pins::SDA);
    gpio_disable_pulls(pins::SCL);
    gpio_set_function(pins::SDA, GPIO_FUNC_I2C);
    gpio_set_function(pins::SCL, GPIO_FUNC_I2C);
    psensor.init();

    cell1_drain.init(true);
    cell1_drain.set(0);
    cell2_drain.init(true);
    cell2_drain.set(0);
    cell3_drain.init(true);
    cell3_drain.set(0);

    boost_ctrl.init(true);
    boost_ctrl.set(0);

    adc_init();
    adc_gpio_init(pins::CELL1_SENSE);
    adc_gpio_init(pins::CELL2_SENSE);
    adc_gpio_init(pins::BATT_SENSE);

    gpio_set_function(pins::NLED, GPIO_FUNC_SIO);
    gpio_set_dir(pins::NLED, true);
    gpio_put(pins::NLED, true);
    gpio_set_function(pins::USB_PWRON, GPIO_FUNC_SIO);
    gpio_set_dir(pins::USB_PWRON, true);
    gpio_put(pins::USB_PWRON, false);
}
