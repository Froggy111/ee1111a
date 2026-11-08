namespace pid {
struct Config {
    float kp;
    float ki;
    float kd;
    float min;
    float max;
};
template <const Config CONF>
class Pid {
   private:
    float prev = 0.0f;
    float integ = 0.0f;
    float setpoint = 0.0f;

   public:
    inline void set_target(float target) { this->setpoint = target; }
    inline void reset(float reading) {
        prev = reading;
        float err = reading - setpoint;
        integ += err;
    }
    inline float update(float reading) {
        float deriv = reading - prev;
        float err = reading - setpoint;
        prev = reading;
        integ += err;
        float output = CONF.kp * err + CONF.kd * deriv + CONF.kp * integ;
        // anti windup
        if (output > CONF.max) {
            float min = CONF.max - (CONF.kp * err + CONF.kd * deriv);
            if (min < 0.0f) {
                min = 0.0f;
            }
            integ = min / CONF.kp;
            output = CONF.max;
        } else if (output < CONF.min) {
            float min = CONF.max - (CONF.kp * err + CONF.kd * deriv);
            if (min > 0.0f) {
                min = 0.0f;
            }
            integ = min / CONF.kp;
            output = CONF.min;
        }
        return output;
    }
};
}  // namespace pid
