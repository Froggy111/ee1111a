#include <hardware/i2c.h>

#include "types.hpp"
using namespace types;

namespace hwdesc {

const u8 I2C_INST = 0;
const u8 CELL1_ADC_CH = 2;
const u8 CELL2_ADC_CH = 1;
const u8 BATT_ADC_CH = 0;
const float VSENSE_SCALE = ((10.0 + 36.0) / 10.0) * (3.3 / (1 << 12));

const u8 INA219B_ADDR = 0b1000000;

namespace pins {
const u8 SDA = 0;
const u8 SCL = 1;
const u8 NLED = 23;
const u8 USB_PWRON = 29;

const u8 CELL1_SENSE = 28;
const u8 CELL2_SENSE = 27;
const u8 BATT_SENSE = 26;

const u8 BOOST_EN = 5;
/// 2A
const u8 BOOST_SW = 4;

namespace drain {
const u8 EN1 = 8;
/// 4B
const u8 NSW1 = 9;
const u8 EN2 = 11;
/// 5A
const u8 NSW2 = 10;
const u8 EN3 = 2;
/// 1B
const u8 NSW3 = 3;
}  // namespace drain
}  // namespace pins

}  // namespace hwdesc
