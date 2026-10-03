/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2024 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
#include "PwmPin.h"

///////////////////

LedC::LedC(uint8_t pin, uint32_t freq, boolean invert, uint8_t resolution){

  if(freq==0)
    freq=DEFAULT_PWM_FREQ;

  int maxRes=LEDC_TIMER_BIT_MAX-1;                                    // find the maximum possible resolution
  while(maxRes>1 && 80.0e6/(freq*pow(2,maxRes))<1)    // stops at 1: ledc_timer_config rejects frequencies that are too high
    maxRes--;

  int res=maxRes;
  if(resolution>0){
    if(resolution>maxRes)
      ESP_LOGW(PWM_TAG,"Resolution=%d bits is not possible at Frequency=%lu Hz - using %d bits",resolution,(unsigned long)freq,maxRes);
    else
      res=resolution;
  }

  for(int nMode=0;nMode<LEDC_SPEED_MODE_MAX;nMode++){
    for(int nChannel=0;nChannel<LEDC_CHANNEL_MAX;nChannel++){
      for(int nTimer=0;nTimer<LEDC_TIMER_MAX;nTimer++){
        if(!channelList[nChannel][nMode] && !channelReserved[nChannel][nMode] && !timerReserved[nTimer][nMode]){
          
          if(!timerList[nTimer][nMode]){                                // if this timer slot is free, use it
            timerList[nTimer][nMode]=new ledc_timer_config_t();         // create new timer instance
            timerList[nTimer][nMode]->speed_mode=(ledc_mode_t)nMode;
            timerList[nTimer][nMode]->timer_num=(ledc_timer_t)nTimer;
            timerList[nTimer][nMode]->freq_hz=freq;
#if defined(SOC_LEDC_SUPPORT_APB_CLOCK)
            timerList[nTimer][nMode]->clk_cfg=LEDC_USE_APB_CLK;
#elif defined(SOC_LEDC_SUPPORT_PLL_DIV_CLOCK)
            timerList[nTimer][nMode]->clk_cfg=LEDC_USE_PLL_DIV_CLK;
#endif

#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 1, 5)
            timerList[nTimer][nMode]->deconfigure=false;
#endif
            
            timerList[nTimer][nMode]->duty_resolution=(ledc_timer_bit_t)res;
            if(ledc_timer_config(timerList[nTimer][nMode])!=0){
              ESP_LOGE(PWM_TAG,"Frequency=%lu Hz is out of allowed range ---",(unsigned long)freq);
              delete timerList[nTimer][nMode];
              timerList[nTimer][nMode]=NULL;
              return;              
            }
          }
          
          if(timerList[nTimer][nMode]->freq_hz==freq && (resolution==0 || timerList[nTimer][nMode]->duty_resolution==res)){     // if timer matches desired frequency and resolution (always true if newly-created above)
            channelList[nChannel][nMode]=new ledc_channel_config_t();       // create new channel instance
            channelList[nChannel][nMode]->speed_mode=(ledc_mode_t)nMode;
            channelList[nChannel][nMode]->channel=(ledc_channel_t)nChannel;
            channelList[nChannel][nMode]->timer_sel=(ledc_timer_t)nTimer;
            channelList[nChannel][nMode]->intr_type=LEDC_INTR_DISABLE;
            channelList[nChannel][nMode]->flags.output_invert=invert;
            channelList[nChannel][nMode]->hpoint=0;
            channelList[nChannel][nMode]->gpio_num=pin;
            timer=timerList[nTimer][nMode];
            channel=channelList[nChannel][nMode];
            return;
          }       
        }
      }
    }
  }
}

///////////////////

boolean LedC::reserveChannel(ledc_mode_t mode, ledc_channel_t channel){

  if((int)mode<0 || (int)mode>=LEDC_SPEED_MODE_MAX || (int)channel<0 || (int)channel>=LEDC_CHANNEL_MAX){
    ESP_LOGE(PWM_TAG,"Can't reserve channel=%d, mode=%d - out of range",(int)channel,(int)mode);
    return(false);
  }

  if(channelList[channel][mode]){
    ESP_LOGE(PWM_TAG,"Can't reserve channel=%d, mode=%d - already assigned",(int)channel,(int)mode);
    return(false);
  }

  channelReserved[channel][mode]=true;
  return(true);
}

///////////////////

boolean LedC::reserveTimer(ledc_mode_t mode, ledc_timer_t timer){

  if((int)mode<0 || (int)mode>=LEDC_SPEED_MODE_MAX || (int)timer<0 || (int)timer>=LEDC_TIMER_MAX){
    ESP_LOGE(PWM_TAG,"Can't reserve timer=%d, mode=%d - out of range",(int)timer,(int)mode);
    return(false);
  }

  if(timerList[timer][mode]){
    ESP_LOGE(PWM_TAG,"Can't reserve timer=%d, mode=%d - already assigned",(int)timer,(int)mode);
    return(false);
  }

  timerReserved[timer][mode]=true;
  return(true);
}

///////////////////

void LedC::setDuty(uint32_t duty){

  channel->duty=duty;

  if(!configured){                                                    // first call requires full configuration of channel (including GPIO)
    if(ledc_channel_config(channel)==ESP_OK)
      configured=true;
  } else {                                                            // subsequent calls only need to update duty (much faster, and glitch-free since update occurs at start of next PWM cycle)
    ledc_set_duty(channel->speed_mode,channel->channel,duty);
    ledc_update_duty(channel->speed_mode,channel->channel);
  }
}

///////////////////

LedPin::LedPin(uint8_t pin, float level, uint32_t freq, boolean invert, uint8_t resolution) : LedC(pin, freq, invert, resolution){
  
  if(!channel){
    ESP_LOGE(PWM_TAG,"Can't create LedPin(%d) - no open PWM channels and/or Timers",pin);
    return;
  }
  else
    ESP_LOGI(PWM_TAG,"LedPin=%d: mode=%d, channel=%d, timer=%d, freq=%lu Hz, resolution=%d bits %s",
      channel->gpio_num,
      channel->speed_mode,
      channel->channel,
      channel->timer_sel,
      (unsigned long)timer->freq_hz,
      timer->duty_resolution,
      channel->flags.output_invert?"(inverted)":""
      );

  if(!fadeInitialized){
    ledc_fade_func_install(0);
    fadeInitialized=true;
  }
  ledc_cbs_t fadeCallbackList = {.fade_cb = fadeCallback};                          // for some reason, ledc_cb_register requires the function to be wrapped in a structure
  ledc_cb_register(channel->speed_mode,channel->channel,&fadeCallbackList,this);

  set(level);   
}

///////////////////

boolean LedPin::cancelPending(){

  portENTER_CRITICAL(&pendingMux);
  boolean wasQueued=pendingQueued;
  pendingValid=false;
  pendingQueued=false;
  portEXIT_CRITICAL(&pendingMux);

  return(wasQueued);
}

///////////////////

void LedPin::set(float level){

  if(!channel)
    return;

  if(level>100)
    level=100;

  if(level<0)
    level=0;

  std::lock_guard<std::recursive_mutex> lock(mux);

  boolean wasQueued=cancelPending();                // a pending request is always discarded
  uint32_t duty=level*maxDuty()/100.0;

  if(fadeState==FADING && !wasQueued){              // a fade is in progress: stop it
#if SOC_LEDC_SUPPORT_FADE_STOP
    ledc_fade_stop(channel->speed_mode,channel->channel);
    setDuty(duty);
    fadeState=NOT_FADING;
#else
    channel->duty=duty;                             // no fade stop: stop it by fully re-configuring the channel (same behavior as before)
    ledc_channel_config(channel);
#endif
    return;
  }

  if(wasQueued)                                     // the fade had already ended and its pending successor never started
    fadeState=NOT_FADING;

  setDuty(duty);
}

///////////////////

int LedPin::startFade(float level, uint32_t fadeTime, int fadeType){

  float d=level*maxDuty()/100.0;

  if(fadeType==PROPORTIONAL)
    fadeTime*=fabs((float)ledc_get_duty(channel->speed_mode,channel->channel)-d)/(float)maxDuty();

  fadeStartMs=millis();
  fadeDurationMs=fadeTime;
  fadeState=FADING;
  if(ledc_set_fade_time_and_start(channel->speed_mode,channel->channel,d,fadeTime,LEDC_FADE_NO_WAIT)!=ESP_OK){
    fadeState=NOT_FADING;
    return(1);
  }
  return(0);
}

///////////////////

int LedPin::fade(float level, uint32_t fadeTime, int fadeType){

  if(!channel)
    return(1);

  if(level>100)
    level=100;

  if(level<0)
    level=0;

  std::lock_guard<std::recursive_mutex> lock(mux);

  if(fadeOverdue()){                                // the end-of-fade callback never arrived: consider the fade finished
    ESP_LOGW(PWM_TAG,"LedPin=%d: fade end not detected - assuming fade is finished",channel->gpio_num);
#if SOC_LEDC_SUPPORT_FADE_STOP
    ledc_fade_stop(channel->speed_mode,channel->channel);     // harmless if the fade has really ended; return value ignored
#endif
    cancelPending();
    fadeState=NOT_FADING;
  }

  if(fadeState==FADING){                            // fading already in progress
#if SOC_LEDC_SUPPORT_FADE_STOP
    if(ledc_fade_stop(channel->speed_mode,channel->channel)!=ESP_OK)
      return(1);
    fadeState=NOT_FADING;
#else
    if(!startFadeTask())
      return(1);

    boolean stored=false;
    portENTER_CRITICAL(&pendingMux);
    if(fadeState==FADING){                          // re-check: the fade may have ended in the meantime
      pendingLevel=level;
      pendingTime=fadeTime;
      pendingType=fadeType;
      pendingValid=true;                            // replaces any previous request
      stored=true;
    }
    portEXIT_CRITICAL(&pendingMux);

    if(stored)
      return(0);
#endif
  }

  return(startFade(level,fadeTime,fadeType));
}

///////////////////

int LedPin::fadeStatus(){

  if(!channel)
    return(NOT_FADING);

  std::lock_guard<std::recursive_mutex> lock(mux);

  if(fadeOverdue()){                                // the end-of-fade callback never arrived: consider the fade finished
#if SOC_LEDC_SUPPORT_FADE_STOP
    ledc_fade_stop(channel->speed_mode,channel->channel);     // harmless if the fade has really ended; return value ignored
#endif
    cancelPending();
    fadeState=NOT_FADING;
  }

  int state=fadeState;

  if(state==COMPLETED){
    fadeState=NOT_FADING;
    return(COMPLETED);
  }

  return((state==FADING || pendingValid)?FADING:state);
}

///////////////////

boolean LedPin::isFading(){
  if(fadeOverdue())                                 // read-only check: same rule as fade(), without changing the state
    return(false);
  return(fadeState==FADING || pendingValid);
}

///////////////////

float LedPin::getLevel(){

  if(!channel || !configured)
    return(0);

  std::lock_guard<std::recursive_mutex> lock(mux);
  return(ledc_get_duty(channel->speed_mode,channel->channel)*100.0/maxDuty());
}

///////////////////

LedPin *LedPin::setFadeCallback(void (*f)(LedPin *, void *), void *arg){

  std::lock_guard<std::recursive_mutex> lock(mux);
  portENTER_CRITICAL(&pendingMux);                  // function and argument change together, as seen by the ISR
  endCallback=f;
  endArg=arg;
  portEXIT_CRITICAL(&pendingMux);
  return(this);
}

///////////////////

bool IRAM_ATTR LedPin::fadeCallback(const ledc_cb_param_t *param, void *arg){

  LedPin *p=(LedPin *)arg;
  boolean pending;

  portENTER_CRITICAL_ISR(&p->pendingMux);
  void (*cb)(LedPin *, void *)=p->endCallback;      // read once, together with its argument
  void *cbArg=p->endArg;
  pending=p->pendingValid;
  if(pending)
    p->pendingQueued=true;                          // state stays FADING: the ledFade task starts the pending fade
  else
    p->fadeState=COMPLETED;
  portEXIT_CRITICAL_ISR(&p->pendingMux);

  if(!pending){
    if(cb)
      cb(p,cbArg);
    return(false);
  }

#if !SOC_LEDC_SUPPORT_FADE_STOP
  BaseType_t woken=pdFALSE;
  if(fadeQueue && xQueueSendFromISR(fadeQueue,&p,&woken)==pdTRUE)
    return(woken==pdTRUE);

  portENTER_CRITICAL_ISR(&p->pendingMux);           // queue full: discard the pending request
  p->pendingValid=false;
  p->pendingQueued=false;
  p->fadeState=COMPLETED;
  portEXIT_CRITICAL_ISR(&p->pendingMux);

  if(cb)
    cb(p,cbArg);
#endif

  return(false);
}

///////////////////

#if !SOC_LEDC_SUPPORT_FADE_STOP

static std::mutex fadeTaskMux;

boolean LedPin::startFadeTask(){

  std::lock_guard<std::mutex> lock(fadeTaskMux);

  if(fadeQueue)
    return(true);

  QueueHandle_t q=xQueueCreate((int)LEDC_CHANNEL_MAX*(int)LEDC_SPEED_MODE_MAX,sizeof(LedPin *));
  if(!q){
    ESP_LOGE(PWM_TAG,"Can't create fade queue");
    return(false);
  }

  if(xTaskCreate(fadeTask,"ledFade",3072,q,2,NULL)!=pdPASS){
    vQueueDelete(q);
    ESP_LOGE(PWM_TAG,"Can't create ledFade task");
    return(false);
  }

  fadeQueue=q;
  return(true);
}

///////////////////

void LedPin::fadeTask(void *arg){

  QueueHandle_t q=(QueueHandle_t)arg;
  LedPin *p;

  for(;;){
    if(xQueueReceive(q,&p,portMAX_DELAY)==pdTRUE)
      p->startPending();
  }
}

///////////////////

void LedPin::startPending(){

  std::lock_guard<std::recursive_mutex> lock(mux);

  float level;
  uint32_t fadeTime;
  int fadeType;
  boolean valid;

  portENTER_CRITICAL(&pendingMux);                  // copy fields only
  valid=pendingValid && pendingQueued;             // a request not yet handed over by the ISR must wait for the current fade to end
  level=pendingLevel;
  fadeTime=pendingTime;
  fadeType=pendingType;
  if(valid){
    pendingValid=false;
    pendingQueued=false;
  }
  portEXIT_CRITICAL(&pendingMux);

  if(valid)                                         // otherwise the request was cancelled or is not yet startable (stale queue entry)
    startFade(level,fadeTime,fadeType);
}

#endif

///////////////////


void LedPin::HSVtoRGB(float h, float s, float v, float *r, float *g, float *b ){

  // The algorithm below was provided on the web at https://www.cs.rit.edu/~ncs/color/t_convert.html
  // h = [0,360]
  // s = [0,1]
  // v = [0,1]

  int i;
  float f, p, q, t;
  
  if( s == 0 ){
    *r = *g = *b = v;
    return;
  }
  
  h=fmodf(h,360);
  if(h<0)
    h+=360;
  h /= 60;
  i = floor( h ) ;
  f = h - i;
  p = v * ( 1 - s );
  q = v * ( 1 - s * f );
  t = v * ( 1 - s * ( 1 - f ) );
  switch( i % 6 ) {
    case 0:
      *r = v;
      *g = t;
      *b = p;
      break;
    case 1:
      *r = q;
      *g = v;
      *b = p;
      break;
    case 2:
      *r = p;
      *g = v;
      *b = t;
      break;
    case 3:
      *r = p;
      *g = q;
      *b = v;
      break;
    case 4:
      *r = t;
      *g = p;
      *b = v;
      break;
    case 5:
      *r = v;
      *g = p;
      *b = q;
      break;
  }
}

////////////////////////////

ServoPin::ServoPin(uint8_t pin, double initDegrees, uint16_t minMicros, uint16_t maxMicros, double minDegrees, double maxDegrees) : LedC(pin, 50){
  
  if(!channel)
    ESP_LOGE(PWM_TAG,"Can't create ServoPin(%d) - no open PWM channels and/or Timers",pin);
  else
    ESP_LOGI(PWM_TAG,"ServoPin=%d: mode=%d channel=%d, timer=%d, freq=%d Hz, resolution=%d bits",
      channel->gpio_num,
      channel->speed_mode,
      channel->channel,
      channel->timer_sel,
      timer->freq_hz,
      timer->duty_resolution
      );
            
  this->minMicros=minMicros;
  this->maxMicros=maxMicros;
  this->minDegrees=minDegrees;
  microsPerDegree=(double)(maxMicros-minMicros)/(maxDegrees-minDegrees);

  set(initDegrees);
      
}

///////////////////

void ServoPin::set(double degrees){

  if(!channel)
    return;

  if(!isnan(degrees)){
    double usec=(degrees-minDegrees)*microsPerDegree+minMicros;
    
    if(usec<minMicros)
      usec=minMicros;
    else if(usec>maxMicros)
      usec=maxMicros;
  
    usec*=timer->freq_hz/1e6*maxDuty();
  
    setDuty(usec);
  } else {
    setDuty(0);
  }
}

////////////////////////////

ledc_channel_config_t *LedC::channelList[LEDC_CHANNEL_MAX][LEDC_SPEED_MODE_MAX]={};
ledc_timer_config_t *LedC::timerList[LEDC_TIMER_MAX][LEDC_SPEED_MODE_MAX]={};
boolean LedC::channelReserved[LEDC_CHANNEL_MAX][LEDC_SPEED_MODE_MAX]={};
boolean LedC::timerReserved[LEDC_TIMER_MAX][LEDC_SPEED_MODE_MAX]={};
boolean LedPin::fadeInitialized=false;
#if !SOC_LEDC_SUPPORT_FADE_STOP
QueueHandle_t LedPin::fadeQueue=NULL;
#endif
