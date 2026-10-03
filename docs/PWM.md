# Pulse Width Modulation (PWM)

The ESP32 has up to 16 PWM channels that can be used to drive a variety of devices.  HomeSpan includes an integrated PWM library with dedicated classes designed for controlling **Dimmable LEDs** as well as **Servo Motors**.

## *LedPin(uint8_t pin [,float level [,uint32_t frequency [,boolean invert [,uint8_t resolution]]]])*

Creating an instance of this **class** configures the specified *pin* to output a PWM signal suitable for a controlling dimmable LED.  Arguments, along with their defaults if left unspecified, are as follows:

  * *pin* - the pin on which the PWM control signal will be output
  * *level* - sets the initial %duty-cycle of the PWM from from 0 (LED completely off) to 100 (LED fully on).  Default=0 (LED initially off)
  * *frequency* - sets the PWM frequency, in Hz, as a 32-bit value.  The usable range depends on the chip and on the resolution (the hardware divides an 80 MHz clock).  Defaults to 5000 Hz if unspecified, or if set to 0
  * *invert* - if true, the output of the PWM signal will be inverted.  Default=false
  * *resolution* - sets the duty-cycle resolution of the timer, in bits.  Default=0, which selects the maximum resolution possible for the frequency.  If the resolution requested is higher than the maximum possible for the frequency, the maximum is used and a warning message is output.  A Timer is shared with another LedPin or ServoPin only if the frequency is the same and either *resolution* is 0 or it equals the resolution of that Timer
 
 The following methods are supported:

* `void set(float level)`

  * sets the PWM %duty-cycle to *level*, where *level* ranges from 0 (LED completely off) to 100 (LED fully on)

* `int fade(float level, uint32_t fadeTime, int fadeType=LedPin::ABSOLUTE)`

  * uses the ESP32's PWM hardware to smoothly fade the LED to *level* (from 0-100) over a maximum of *fadeTime* milliseconds
  * if *fadeType* is set to **LedPin::ABSOLUTE** (the default), fading will take the full amount of time specified by *fadeTime*
  * if *fadeType* is set to **LedPin::PROPORTIONAL**, the fading time will be scaled down proportionally according to the difference between the current level and the level specified.  For example, if the current level is set to 30, then
    * `fade(20, 1000, LedPin::ABSOLUTE)` sets the level to 20 over the course of 1 second, whereas
    * `fade(20, 1000, LedPin::PROPORTIONAL)` sets the level to 20 over the course of 100 milliseconds (since the level only needs to change by 10 out of 100 units)
  * this is a **NON-BLOCKING** method and will return immediately.  Fading occurs in the background controlled by the ESP32 hardware
  * if a fade is already in progress, *fade()* sets a new destination without blocking the caller.  The behavior depends on the chip:
    * chips that support stopping a fade (ESP32-S2, S3, C3, C5, C6): the fade in progress is stopped and the new fade starts at once from the current level
    * ESP32 (original): the hardware cannot change a fade in progress.  The request is stored as *pending* and starts automatically as soon as the current fade ends, from a shared background task named *ledFade* (created on the first pending request).  A newer pending request replaces an older one, so only the last request is executed.  Until the last fade ends, `fadeStatus()` and `isFading()` report fading
  * this method returns 0 if the fading has started or the request has been accepted, or 1 if the fade could not be started
  * calling `set()` while a fade is in progress ends the fade and discards any pending request
  * use the *fadeStatus* method (below) to determine the current fading status of any given LedPin

* `int fadeStatus()`

  * returns the fading status of an LedPin.  Return values are as follows:
  
    * **LedPin::NOT_FADING** - the LedPin is not currently fading
    * **LedPin::FADING** - fading on LedPin is currently in progress and cannot be changed/stopped
    * **LedPin::COMPLETED** - fading has just completed  
      * once this value is returned, subsequent calls to `fadeStatus()` will return **LedPin::NOT_FADING** (unless you called `fade()` again)
      * by checking for `fadeStatus()==LedPin::COMPLETED` in a `loop()` method, you can thus trigger a new action (if desired) once fading is completed
  
* `boolean isFading()`

  * returns true if a fade is in progress or a fade request is pending, false otherwise
  * this method only reads the state (unlike `fadeStatus()`, it never changes it) and can be called from any task

* `float getLevel()`

  * returns the current level, from 0 to 100, read from the PWM hardware (returns 0 if the LedPin was not successfully initialized)

* `LedPin *setFadeCallback(void (*f)(LedPin *, void *), void *arg=NULL)`

  * registers the function *f*, which is called with the LedPin and *arg* when a fade ends and no pending request follows it.  Returns the LedPin itself, so calls can be chained
  * **the function runs in interrupt (ISR) context**: keep it short, do not call blocking functions, and do not log.  Functions that run in ISR context should be placed in IRAM with `IRAM_ATTR`

* `int getPin()`

  * returns the pin number (or -1 if LedPin was not successfully initialized)

* `uint8_t getResolution()`

  * returns the resolution of the Timer, in bits (0 if the LedPin was not successfully initialized)

* `uint32_t getFreq()`

  * returns the frequency of the Timer, in Hz (0 if the LedPin was not successfully initialized)

The methods `set()`, `fade()`, `fadeStatus()` and `getLevel()` can be called from different tasks on the same LedPin.  Each LedPin has its own recursive mutex, so concurrent calls on the same object are serialized.  No mutex is taken in ISR context.
  
LedPin also includes a static class function that converts Hue/Saturation/Brightness values (typically used by HomeKit) to Red/Green/Blue values (typically used to control multi-color LEDS).

* `static void HSVtoRGB(float h, float s, float v, float *r, float *g, float *b)`

  * *h* - input Hue value, range 0-360
  * *s* - input Saturation value, range 0-1
  * *v* - input Brightness value, range 0-1
  * *r* - output Red value, range 0-1
  * *g* - output Green value, range 0-1
  * *b* - output Blue value, range 0-1

See tutorial sketch [#10 (RGB_LED)](../examples/10-RGB_LED) for an example of using LedPin to control an RGB LED.  Also see [*File → Examples → HomeSpan → Other Examples → FadingLED*](../examples/Other%20Examples/FadingLED) for an example of to use the ESP32's built-in fading controls.

## *ServoPin(uint8_t pin [,double initDegrees [,uint16_t minMicros, uint16_t maxMicros, double minDegrees, double maxDegrees]])*

Creating an instance of this **class** configures the specified *pin* to output a 50 Hz PWM signal, which is suitable for controlling most Servo Motors.  There are three forms of the constructor: one with just a single argument; one with two arguments; and one with all six arguments.  Arguments, along with their defaults if left unspecified, are as follows:

  * *pin* - the pin on which the PWM control signal will be output.  The control wire of a Servo Motor should be connected this pin
  * *initDegrees* - the initial position (in degrees) to which the Servo Motor should be set (default=0°)
  * *minMicros* - the pulse width (in microseconds) that moves the Servo Motor to its "minimium" position of *minDegrees* (default=1000𝛍s)
  * *maxMicros* - the pulse width (in microseconds) that moves the Servo Motor to its "maximum" position of *maxDegrees* (default=2000𝛍s)
  * *minDegrees* - the position (in degrees) to which the Servo Motor moves when receiving a pulse width of *minMicros* (default=-90°)
  * *maxDegrees* - the position (in degrees) to which the Servo Motor moves when receiving a pulse width of *maxMicros* (default=90°)

The *minMicros* parameter must be less than the *maxMicros* parameter, but setting *minDegrees* to a value greater than *maxDegrees* is allowed and can be used to reverse the minimum and maximum positions of the Servo Motor. The following methods are supported:

* `void set(double position)`

  * sets the position of the Servo Motor to *position* (in degrees).  In order to protect the Servo Motor, values of *position* less than *minDegrees* are automatically reset to *minDegrees*, and values greater than *maxDegrees* are automatically reset to *maxDegrees*
  * if *position* is specified as *NAN* (i.e. the C++ "not-a-number" constant), the duty-cycle is set to zero, which effectively stops the pulse generation --- for most analog servos this means the motor can be freely rotated.  Calling `set()` once again with *position* equal to an actual number of degrees restarts the pulse train and sets the servo position accordingly
  
* `int getPin()`

  * returns the pin number (or -1 if ServoPin was not successfully initialized)

A worked example showing how ServoPin can be used to control the Horizontal Tilt of a motorized Window Shade can be found in the Arduino IDE under [*File → Examples → HomeSpan → Other Examples → ServoControl*](../examples/Other%20Examples/ServoControl).

### PWM Resource Allocation and Limitations

The following PWM resources are available:

* ESP32: 16 Channels / 8 Timers (arranged in two distinct sets of 8 Channels and 4 Timers)
* ESP32-S2: 8 Channels / 4 Timers
* ESP32-C3: 6 Channels / 4 Timers
* ESP32-S3: 8 Channels / 4 Timers

HomeSpan *automatically* allocates Channels and Timers to LedPin and ServoPin objects as they are instantiated. Every pin assigned consumes a single Channel;  every *unique* frequency specified among all channels (within the same set, for the ESP32) consumes a single Timer.  HomeSpan will conserve resources by re-using the same Timer for all Channels operating at the same frequency.  *HomeSpan also automatically configures each Timer to support the maximum duty-resolution possible for the frequency specified.*

#### Sharing PWM Resources with Other Code

If your sketch also uses the LEDC hardware directly (ESP-IDF calls, or Arduino functions such as `ledcAttach()` and `analogWrite()`), reserve the Channels and Timers you use so HomeSpan never assigns them:

* `static boolean LedC::reserveChannel(ledc_mode_t mode, ledc_channel_t channel)`
* `static boolean LedC::reserveTimer(ledc_mode_t mode, ledc_timer_t timer)`

Both methods return true if the reservation succeeds.  They return false, and output an error message, if the index is out of range or if the resource has already been assigned to a LedPin or ServoPin.  Call them before creating any LedPin or ServoPin.  A reserved Timer is never shared, even if its frequency matches.  For example:

```C++
void setup(){
  LedC::reserveChannel(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);     // used below by my own code
  LedC::reserveTimer(LEDC_LOW_SPEED_MODE,LEDC_TIMER_0);

  ledcAttachChannel(4,1000,10,0);                               // Arduino API: pin 4, 1 kHz, 10 bits, channel 0 (timer assignment is made by Arduino)

  new LedPin(18,0,39062,false,11);                              // HomeSpan allocates other resources
}
```

Note that Arduino selects its own Timer when attaching a channel, so reserve the Timer that your code actually uses.

#### Diagnostic Messages

The **LedPin** and **ServoPin** classes output *Information \[I\]* and *Warning \[W\]* messages to the Serial Monitor based on the *Core Debug Level* selected when compiling the sketch using the Arduino IDE.  A non-fatal warning message is produced when insufficient Channel or Timer resources prevent the creation of a new LedPin or ServoPin object.  Calls to the `set()` method for objects that failed to be properly created are silently ignored.

---

[↩️](../README.md) Back to the Welcome page
