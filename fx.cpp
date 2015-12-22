#include <samplerate.h>

#include <QDebug>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QMouseEvent>
#include <QDial>
#include <QDoubleSpinBox>
#include <QDrag>
#include <QMimeData>

#include <QXmlStreamWriter>
#include <QXmlStreamReader>


#include "fx.h"
#include "utils.h"
#include "3pass_eq.h"


void CRingbuffer::put (float f)
{
  head++;

  if (head >= samples_max)
     head = 0;

  current_size++;

  buffer[head] = f;
}


float CRingbuffer::get()
{
  if (current_size == 0)
     return ret_value;

  tail++;

  if (tail >= samples_max)
     tail = 0;

  current_size--;

  return buffer[tail];
}


CRingbuffer::CRingbuffer (size_t samples, float def_value)
{
  ret_value = def_value;
  buffer = 0;
  samples_max = 0;
  current_size = 0;
  head = 0;
  tail = 0;

  prepare_buffer (samples);
}


void CRingbuffer::prepare_buffer (size_t samples)
{
  qDebug() << "CRingbuffer::prepare_buffer " << samples;

  if (buffer)
     delete buffer;

  buffer = new float [samples]; 
  samples_max = samples;

  qDebug() << "CRingbuffer::prepare_buffer - done ";
}


CRingbuffer::~CRingbuffer()
{
  if (buffer)
     delete buffer;
}


AFx::AFx (int srate, int chann)
{
//  qDebug() << "AFx::AFx";

  bypass = false;
  realtime = true;
  ui_visible = false;
  buffer = 0;
  wnd_ui = 0;
  state = FXS_STOP;
  name = "AFx";

  samplerate = srate;
  channels = chann;
}


AFx::~AFx()
{
  if (wnd_ui)
     {
      wnd_ui->close();
      delete wnd_ui;
     }
}


void AFx::show_ui()
{
  if (wnd_ui)
     wnd_ui->setVisible (! wnd_ui->isVisible());
}


CFxList::CFxList()
{
  list.append (new CFxSimpleAmp (1, 1));
  list.append (new CFxSimpleEQ (1, 1));
  list.append (new CFxSimpleOverdrive (1, 1));
  list.append (new CFxPitchShift (1, 1));
  list.append (new CFxSimpleNoiseGate (1, 1));
  list.append (new CFxSimpleLimiter (1, 1));
//  list.append (new CFxStereoRotator (1, 1));
  
  
  
  
//  list.append (new CFxSimpleComp (1, 1));

//  list.append (new CFxSimpleDelay (1, 1));

}


CFxList::~CFxList()
{
  foreach (AFx *f, list) 
          {
           delete f; 
          }
}
 

AFx *CFxList::find_by_name (const QString &fxname)
{
  for (int i = 0; i < list.size(); i++)
      {
       if (list[i]->name == fxname) 
          return list[i];
      }    

  return 0;
}



QStringList CFxList::names()
{
  QStringList l;
  foreach (AFx *f, list)
          l.append (f->name);
  return l;        
}


CFxSimpleAmp::CFxSimpleAmp (int srate, int chann): AFx (srate, chann)
{
  name = "FxSimpleAmp";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple Amp"));

  gain = 1.0f;

  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  label = new QLabel (tr ("Gain: 1 dB"));

  QDial *dial_gain = new QDial;
  dial_gain->setWrapping (false);
  connect (dial_gain, SIGNAL(valueChanged(int)), this, SLOT(dial_gain_valueChanged(int)));
  dial_gain->setRange (-26, 26);

  dial_gain->setValue (0);

  v_box->addWidget (label);
  v_box->addWidget (dial_gain);
}


void CFxSimpleAmp::dial_gain_valueChanged (int value)
{
  float f = 1.0f;
  if (value == 0)
     {
      gain = 1.0f;
      return;
     }

   f = db2lin (value);

   gain = f;

   label->setText (tr ("Gain: %1 dB").arg (value));

   qDebug() << gain;
}


CFxSimpleAmp::~CFxSimpleAmp()
{
//  qDebug() << "~CFxSimpleAmp()";
}


AFx* CFxSimpleAmp::self_create (int srate, int chann)
{
  return new CFxSimpleAmp (srate, chann);
}


size_t CFxSimpleAmp::execute (float *samples, size_t length)
{
  for (size_t i = 0; i < length; i++)
      samples[i] *= gain;

  return length;
}


size_t CFxSimpleEQ::execute (float *samples, size_t length)
{
  if (channels == 1)
     for (size_t i = 0; i < length; i++)
          samples[i] = do_3band (&eq_state_ch00, samples[i]);

  if (channels == 2)
     {
      size_t i = 0;
      while (i < length)
            {
             samples[i] = do_3band (&eq_state_ch00, samples[i]);
             i++;
             samples[i] = do_3band (&eq_state_ch01, samples[i]);
             i++;
            }
     }

  return length;
}


CFxSimpleEQ::CFxSimpleEQ (int srate, int chann): AFx (srate, chann)
{
  name = "CFxSimpleEQ";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("EQ"));

  eq_state_ch00.lg = 1.0;
  eq_state_ch00.mg = 1.0;
  eq_state_ch00.hg = 1.0;

  eq_state_ch01.lg = 1.0;
  eq_state_ch01.mg = 1.0;
  eq_state_ch01.hg = 1.0;

  init_3band_state (&eq_state_ch00, 880, 5000, samplerate * 100);
  init_3band_state (&eq_state_ch01, 880, 5000, samplerate * 100);

//  init_3band_state (&eq_state_ch00, 880, 5000, srate * 100);
//  init_3band_state (&eq_state_ch01, 880, 5000, srate * 100);


  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("Low"));
  QDial *dial_low = new QDial;
  dial_low->setWrapping (false);
  connect (dial_low, SIGNAL(valueChanged(int)), this, SLOT(dial_low_valueChanged(int)));
  dial_low->setRange (-600, 600);

  dial_low->setValue (0);

  v_box->addWidget (l);
  v_box->addWidget (dial_low);


  l = new QLabel (tr ("Mid"));
  QDial *dial_mid = new QDial;
  dial_mid->setWrapping (false);
  connect (dial_mid, SIGNAL(valueChanged(int)), this, SLOT(dial_mid_valueChanged(int)));
  dial_mid->setRange (-600, 600);
  dial_mid->setValue (0);

  v_box->addWidget (l);
  v_box->addWidget (dial_mid);


  l = new QLabel (tr ("Hi"));
  QDial *dial_hi = new QDial;
  dial_hi->setWrapping (false);
  connect (dial_hi, SIGNAL(valueChanged(int)), this, SLOT(dial_hi_valueChanged(int)));
  dial_hi->setRange (-600, 600);
  dial_hi->setValue (0);

  v_box->addWidget (l);
  v_box->addWidget (dial_hi);
}


CFxSimpleEQ::~CFxSimpleEQ()
{
//  qDebug() << "~CFxSimpleEQ()";
}


AFx* CFxSimpleEQ::self_create (int srate, int chann)
{
  return new CFxSimpleEQ (srate, chann);
}


void CFxSimpleEQ::dial_low_valueChanged (int value)
{
  float f = 1;

  if (value == 0)
     {
      eq_state_ch00.lg = 1;
      eq_state_ch01.lg = 1;
      return;
     }

   f = db2lin ((float) value / 100);

   eq_state_ch00.lg = f;
   eq_state_ch01.lg = f;
}


void CFxSimpleEQ::dial_mid_valueChanged (int value)
{
  float f = 1;

  if (value == 0)
     {
      eq_state_ch00.mg = 1;
      eq_state_ch01.mg = 1;

      return;
     }

   f = db2lin ((float) value / 100);

   eq_state_ch00.mg = f;
   eq_state_ch01.mg = f;

   //qDebug() << eq_state.mg;
}


void CFxSimpleEQ::dial_hi_valueChanged (int value)
{
  float f = 1;

  if (value == 0)
     {
      eq_state_ch00.hg = 1;
      eq_state_ch01.hg = 1;
      return;
     }

   f = db2lin ((float) value / 100);

   eq_state_ch00.hg = f;
   eq_state_ch01.hg = f;

   //qDebug() << eq_state.hg;
}


CFxSimpleOverdrive::CFxSimpleOverdrive (int srate, int chann): AFx (srate, chann)
{
  name = "FxSimpleOverdrive";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple Overdrive"));

  gain = 1.0;

  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("Gain"));
  QDial *dial_gain = new QDial;
  dial_gain->setWrapping (false);
  connect (dial_gain, SIGNAL(valueChanged(int)), this, SLOT(dial_gain_valueChanged(int)));
  dial_gain->setRange (1, 600);

  dial_gain->setValue (1);

  v_box->addWidget (l);
  v_box->addWidget (dial_gain);
}


void CFxSimpleOverdrive::dial_gain_valueChanged (int value)
{
  float f = 1;
  if (value == 0)
     {
      gain = 1;
      return;
     }

   f = db2lin (value / 10);

   gain = f;

   //qDebug() << gain;
}


CFxSimpleOverdrive::~CFxSimpleOverdrive()
{
  //qDebug() << "~CFxSimpleOverdrive";
}


AFx* CFxSimpleOverdrive::self_create (int srate, int chann)
{
  return new CFxSimpleOverdrive (srate, chann);
}


size_t CFxSimpleOverdrive::execute (float *samples, size_t length)
{
  for (size_t i = 0; i < length; i++)
      {
       samples[i] *= gain;

       if ( float_greater_than (samples[i], 1.0f))
          samples[i] = 1.0f;
       else
           if (float_less_than (samples[i], -1.0f))
              samples[i] = -1.0f;
      }

  return length;
}


void AFx::set_state (FxState s)
{
  state = s;
}


CFxPitchShift::CFxPitchShift (int srate, int chann): AFx (srate, chann)
{
  name = "FxPitchShift";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple Pitchshifter"));

  ratio = 1.0;

  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *label = new QLabel (tr ("Ratio: 1.0"));

  QDoubleSpinBox *spb_ratio = new QDoubleSpinBox;
  spb_ratio->setRange (-12, 12);
  spb_ratio->setSingleStep (0.1);
  spb_ratio->setValue (1.0);
  connect (spb_ratio, SIGNAL(valueChanged (double )), this, SLOT(spb_ratio_changed (double )));

  v_box->addWidget (label);
  v_box->addWidget (spb_ratio);
}


void CFxPitchShift::spb_ratio_changed (double value)
{
  ratio = value;
  //label->setText (tr ("Ratio: %1").arg (ratio));
}


CFxPitchShift::~CFxPitchShift()
{


}


AFx* CFxPitchShift::self_create (int srate, int chann)
{
  return new CFxPitchShift (srate, chann);
}


size_t CFxPitchShift::execute (float *samples, size_t length)
{
  SRC_DATA data;

  data.src_ratio = ratio;

  data.input_frames = length / channels;
  data.output_frames = length / channels;

  float *data_in = new float [length];
  memcpy (data_in, samples, length * sizeof (float));


  data.data_in = data_in;
  data.data_out = samples;

  int q;

  if (realtime)
     q = SRC_LINEAR;
  else
      q = SRC_SINC_BEST_QUALITY;

  int error = src_simple (&data, q, channels);
  if (error)
     {
      qDebug() << src_strerror (error);
      //delete data_out;
      delete data_in;
      return 0;
     }

  delete data_in;
  return length;
}


void CFxSimpleNoiseGate::levelChanged (double d)
{
  level = db2lin (d);
}


CFxSimpleNoiseGate::CFxSimpleNoiseGate (int srate, int chann): AFx (srate, chann)
{
  name = "FxSimpleNoiseGate";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple noise gate"));

  level = db2lin (20);

  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("level (dB): "));
  QDoubleSpinBox *spb_level = new QDoubleSpinBox;
  spb_level->setDecimals (2);
  spb_level->setRange (-120, 0);
  spb_level->setValue (-30);

  connect (spb_level, SIGNAL(valueChanged(double)), this, SLOT(levelChanged(double)));

  v_box->addWidget (l);
  v_box->addWidget (spb_level);
}


AFx* CFxSimpleNoiseGate::self_create (int srate, int chann)
{
  return new CFxSimpleNoiseGate (srate, chann);
}


size_t CFxSimpleNoiseGate::execute (float *samples, size_t length)
{
  for (size_t i = 0; i < length; i++)
      {
       if (fabs (samples[i]) < level)
           samples[i] = 0.0f;
      }

  return length;
}


CFxSimpleLimiter::CFxSimpleLimiter (int srate, int chann): AFx (srate, chann)
{
  name = "FxSimpleLimiter";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple limiter"));


  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("threshold (dB): "));
  QDoubleSpinBox *spb_level = new QDoubleSpinBox;
  spb_level->setDecimals (2);
  spb_level->setRange (-120, 0);
  spb_level->setValue (-6);
  threshold = db2lin (-6);

  connect (spb_level, SIGNAL(valueChanged(double)), this, SLOT(levelChanged(double)));

  v_box->addWidget (l);
  v_box->addWidget (spb_level);
}


AFx* CFxSimpleLimiter::self_create (int srate, int chann)
{
  return new CFxSimpleLimiter (srate, chann);
}


size_t CFxSimpleLimiter::execute (float *samples, size_t length)
{
  for (size_t i = 0; i < length; i++)
      {
    //   if (fabs (samples[i]) > threshold)
       if (float_greater_than (fabs (samples[i]), threshold))
           samples[i] = samples[i] * threshold;
      }

  return length;
}


void CFxSimpleLimiter::levelChanged (double d)
{
  threshold = db2lin (d);
  //qDebug() << threshold;
}


void CFxSimpleDelay::set_state (FxState s)
{
   state = s;

   if (state == FXS_STOP)
      {
       current_delay_sample = 0;
       //jack_ringbuffer_reset (ringbuffer);
      }

}


CFxSimpleDelay::CFxSimpleDelay (int srate, int chann): AFx (srate, chann)
{
  delay = 1 * samplerate * channels;
  size_t ring_buffer_size = sizeof (float) * delay;
  //ringbuffer = jack_ringbuffer_create (ring_buffer_size);

  name = "CFxSimpleDelay";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple delay"));



  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("delay (msecs): "));
  QDoubleSpinBox *spb_delay = new QDoubleSpinBox;
  spb_delay->setDecimals (2);
  spb_delay->setRange (0, 10000);
  spb_delay->setSingleStep (300);
  spb_delay->setValue (1000.0d);

  dfactor = 1000.0d;

  connect (spb_delay, SIGNAL(valueChanged(double)), this, SLOT(delayChanged(double)));

  current_delay_sample = 0;

  //qDebug() << "delay = " << delay;


  v_box->addWidget (l);
  v_box->addWidget (spb_delay);
}


AFx* CFxSimpleDelay::self_create (int srate, int chann)
{
  return new CFxSimpleDelay (srate, chann);
}


size_t CFxSimpleDelay::execute (float *samples, size_t length)
{
 //   qDebug() << "CFxSimpleDelay::execute - start";
/*
  size_t bytes_per_buf = length * sizeof (float);
  jack_ringbuffer_write (ringbuffer, (char*) samples, bytes_per_buf);

  current_delay_sample += length;

  qDebug() << "delay = " << delay;

  qDebug() << "current_delay_sample = " << current_delay_sample;

  if (current_delay_sample >= delay)
     //read and mix
     {
      float *tb = (float *)malloc (bytes_per_buf);
      size_t len = jack_ringbuffer_read (ringbuffer,
                                        (char*)tb,
                                        bytes_per_buf);

      size_t limit = len / sizeof (float);

      for (size_t i = 0; i < limit; i++)
          {
           samples[i] += tb[i];
          }

      free (tb);
     }

  //qDebug() << "CFxSimpleDelay::execute - end";
*/
  return length;
}


void CFxSimpleDelay::delayChanged (double d)
{
 //qDebug() << "CFxSimpleDelay::delayChanged";

 dfactor = d;
 current_delay_sample = 0;

 //jack_ringbuffer_reset (ringbuffer);
 //jack_ringbuffer_free  (ringbuffer);

 delay = d / 1000 * samplerate * channels;

 //qDebug() << "delay = " << delay;

 size_t ring_buffer_size = sizeof (float) * delay;

 //ringbuffer = jack_ringbuffer_create (ring_buffer_size);
}


CFxSimpleDelay::~CFxSimpleDelay()
{
//  jack_ringbuffer_free (ringbuffer);
}


/*
void CFxSimpleAmp::load_from_string (const QString &s)
{

}

QString CFxSimpleAmp::save_to_string()
{


}

   QXmlStreamWriter stream(&output);
     stream.setAutoFormatting(true);
     stream.writeStartDocument();
     ...
     stream.writeStartElement("bookmark");
     stream.writeAttribute("href", "http://qt.nokia.com/");
     stream.writeTextElement("title", "Qt Home");
     stream.writeEndElement(); // bookmark
     ...
     stream.writeEndDocument();





 QXmlStreamReader xml;
   ...
   while (!xml.atEnd()) {
         xml.readNext();
         ... // do processing
   }
   if (xml.hasError()) {
         ... // do error handling
   }


*/

/*
void AFx::set_parameter (const QString &param, const QString &value)
{

}
*/


void AFx::reset_params (int srate, int chann)
{
  samplerate = srate;
  channels = chann;
}


void CFxSimpleDelay::reset_params (int srate, int chann)
{
  AFx::reset_params (srate, chann);
  delayChanged (dfactor);
}




CFxSimpleComp::CFxSimpleComp (int srate, int chann): AFx (srate, chann)
{
  name = "FxSimpleComp";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Simple compressor"));


  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  QLabel *l = new QLabel (tr ("threshold (dB): "));
  QDoubleSpinBox *spb_level = new QDoubleSpinBox;
  spb_level->setDecimals (2);
  spb_level->setRange (-120, 0);
  spb_level->setValue (-6);
  threshold = db2lin (-6);

  connect (spb_level, SIGNAL(valueChanged(double)), this, SLOT(levelChanged(double)));

  v_box->addWidget (l);
  v_box->addWidget (spb_level);
  
  
  l = new QLabel (tr ("ratio: "));
  QDoubleSpinBox *spb_ratio = new QDoubleSpinBox;
  spb_ratio->setDecimals (2);
  spb_ratio->setRange (0, 30);
  spb_ratio->setValue (2);
  threshold = 2;

  connect (spb_ratio, SIGNAL(valueChanged(double)), this, SLOT(ratioChanged(double)));

  v_box->addWidget (l);
  v_box->addWidget (spb_ratio);
}


AFx* CFxSimpleComp::self_create (int srate, int chann)
{
  return new CFxSimpleComp (srate, chann);
}


size_t CFxSimpleComp::execute (float *samples, size_t length)
{
  for (size_t i = 0; i < length; i++)
      {
       if (float_greater_than (fabs (samples[i]), threshold))
           samples[i] = samples[i] / ratio;
      }

  return length;
}


void CFxSimpleComp::ratioChanged (double d)
{
  ratio = d;
  //qDebug() << threshold;
}


void CFxSimpleComp::levelChanged (double d)
{
  threshold = db2lin (d);
  //qDebug() << threshold;
}

/*

void CFxSimpleDelay::set_state (FxState s)
{
   state = s;

   if (state == FXS_STOP)
      {
       current_delay_sample = 0;
       //jack_ringbuffer_reset (ringbuffer);
      }

}
*/





CFxStereoRotator::CFxStereoRotator (int srate, int chann): AFx (srate, chann)
{
  name = "CFxStereoRotator";
  wnd_ui = new QWidget();

  wnd_ui->setWindowTitle (tr ("Stereo Rotator"));

  angle = 180;

  QVBoxLayout *v_box = new QVBoxLayout;
  wnd_ui->setLayout (v_box);

  label = new QLabel (tr ("Angle:"));

  QDial *dial_angle = new QDial;
  //dial_angle->setWrapping (false);
  connect (dial_angle, SIGNAL(valueChanged(int)), this, SLOT(dial_angle_valueChanged(int)));
  dial_angle->setRange (1, 360);

  dial_angle->setValue (1);

  v_box->addWidget (label);
  v_box->addWidget (dial_angle);
  
  angle = 180;
  
  cos_coef = cos (angle);
  sin_coef = sin (angle);
}


void CFxStereoRotator::dial_angle_valueChanged (int value)
{
  angle = value;
  
  cos_coef = cos (angle);
  sin_coef = sin (angle);
}


CFxStereoRotator::~CFxStereoRotator()
{
//  qDebug() << "~CFxSimpleAmp()";
}


AFx* CFxStereoRotator::self_create (int srate, int chann)
{
  return new CFxStereoRotator (srate, chann);
}


size_t CFxStereoRotator::execute (float *samples, size_t length)
{
  if (channels != 2)
     return 0;
     
/*
 * Type : Stereo Field Rotation: by Michael Gruhn
  
 cos_coef = cos(angle);
sin_coef = sin(angle);

// Do this per sample
out_left  = in_left * cos_coef - in_right * sin_coef;
out_right = in_left * sin_coef + in_right * cos_coef; 
 * 
 */      
  
    size_t i = 0;
  
    do
       {
       
        samples[i] = samples[i] * cos_coef - samples[i+1] * sin_coef;
           
        i++;
        samples[i] = samples[i-1] * sin_coef + samples[i] * cos_coef;
                
        i++;
       }
    while (i < length);


  return length;
}
