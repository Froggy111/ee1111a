#pragma once
#include <hardware/i2c.h>

#include "types.hpp"

namespace ina219b {

using namespace types;

namespace addr {
const u8 CONFIG = 0x00;         // RW
const u8 SHUNT_VOLTAGE = 0x01;  // RO
const u8 BUS_VOLTAGE = 0x02;    // RO
const u8 POWER = 0x03;          // RO
const u8 CURRENT = 0x04;        // RO
const u8 CALIBRATION = 0x05;    // RO
}  // namespace addr

enum class Op : u8 { Read = 1, Write = 0 };

enum class BusRange : u8 {
    _16V = 0,
    _32V = 1,
};

enum class ShuntRange : u8 {
    _40mV = 0b00,
    _80mV = 0b01,
    _160mV = 0b10,
    _320mV = 0b11,
};

enum class SamplingMode : u8 {
    _9bit = 0b0000,    // 64us
    _10bit = 0b0001,   // 148us
    _11bit = 0b0010,   // 276us
    _12bit = 0b0011,   // 532us
    _2xOS = 0b1001,    // 1.06ms
    _4xOS = 0b1010,    // 2.13ms
    _8xOS = 0b1011,    // 4.26ms
    _16xOS = 0b1100,   // 8.51ms
    _32xOS = 0b1101,   // 17.02ms
    _64xOS = 0b1110,   // 34.05ms
    _128xOS = 0b1111,  // 68.10ms
};

enum class OperatingMode : u8 {
    PowerDown = 0b000,
    ShuntTrigger = 0b001,
    BusTrigger = 0b010,
    ShuntBusTrigger = 0b011,
    AdcOff = 0b100,
    ShuntContinuous = 0b101,
    BusContinuous = 0b110,
    ShuntBusContinuous = 0b111,
};

struct Config {
    u8 addr;
    BusRange bus_range;
    ShuntRange shunt_range;
    SamplingMode bus_sampling_mode;
    SamplingMode shunt_sampling_mode;
    OperatingMode operating_mode;
    float resistance;
    float current_lsb;
};

constexpr float current_lsb(float max_current) {
    return max_current / (1 << 15);
}

struct Status {
    // LSB = 4mV
    u16 bus_voltage;
    bool overflow;
    bool conv_ready;
};

template <const Config CONF>
class Ina219B {
   private:
    i2c_inst* i2c;
    inline void write(const u8 addr, u16 val) {
        const u8 i2c_addr = CONF.addr | ((u8)Op::Write << 7);
        u8 buf[4];
        buf[0] = i2c_addr;
        buf[1] = addr;
        // BE
        buf[2] = val >> 8;
        buf[3] = val & 0xFF;
        i2c_write_blocking(i2c, i2c_addr, buf, sizeof(buf), false);
    }
    inline u16 read(const u8 addr) {
        const u8 i2c_addr = CONF.addr | ((u8)Op::Read << 7);
        u8 buf[2] = {i2c_addr, addr};
        i2c_write_blocking(i2c, CONF.addr, buf, sizeof(buf),
                           true);  // middle of transaction
        i2c_read_blocking(i2c, CONF.addr, buf, sizeof(u16), false);
        // BE
        return buf[1] & ((u16)buf[0] << 8);
    }
    constexpr u16 calibration_val() {
        return (u16)(0.04096 / (CONF.current_lsb * CONF.resistance));
    }

   public:
    inline Ina219B(i2c_inst* i2c_) { i2c = i2c_; }
    /// requires i2c to be already initialised
    inline void init() {
        const u16 conf_reg_val =
            ((u8)CONF.bus_range << 13) | ((u8)CONF.shunt_range << 11) |
            ((u8)CONF.bus_sampling_mode << 7) |
            ((u8)CONF.shunt_sampling_mode << 3) | ((u8)CONF.operating_mode);
        this->write(addr::CONFIG, conf_reg_val);
        this->write(addr::CALIBRATION, this->calibration_val());
    }
    // in units of 10uV
    inline i16 read_shunt() { return (i16)this->read(addr::SHUNT_VOLTAGE); }
    // in units of 4mV
    inline Status read_status_bus() {
        Status status;
        auto val = this->read(addr::BUS_VOLTAGE);
        status.bus_voltage = val >> 3;
        status.conv_ready = val & 0b10;
        status.overflow = val & 0b1;
        return status;
    }
    // in units of `CONF.current_lsb`
    inline i16 read_current() { return (i16)this->read(addr::CURRENT); }
    // in units of `CONF.current_lsb * 20`
    inline i16 read_power() { return (i16)this->read(addr::POWER); }
};

}  // namespace ina219b
