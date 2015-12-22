/***************************************************************************
 *   2010 - 2015 by Peter Semiletov                                        *
 *   tea@list.ru                                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   aint with this program; if not, write to the                          *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QTime>
#include <QVBoxLayout>
#include <QUrl>
#include <QMimeData>
#include <QPushButton>
#include <QToolButton>
#include <QDial>

#include <math.h>

#include <samplerate.h>

#include "document.h"
#include "utils.h"
#include "gui_utils.h"

#include "logmemo.h"



enum EMultiChannelMappingWAV //wav, flac, mp3
{
 SPEAKER_FRONT_LEFT = 0,
 SPEAKER_FRONT_RIGHT,
 SPEAKER_FRONT_CENTER,
 SPEAKER_LOW_FREQUENCY, //subwoofer
 SPEAKER_BACK_LEFT,
 SPEAKER_BACK_RIGHT,
 SPEAKER_FRONT_LEFT_OF_CENTER,
 SPEAKER_FRONT_RIGHT_OF_CENTER,
 SPEAKER_BACK_CENTER,
 SPEAKER_SIDE_LEFT,
 SPEAKER_SIDE_RIGHT,
 SPEAKER_LEFT_HEIGHT,
 SPEAKER_RIGHT_HEIGHT
};


QSettings *settings;
CDSP *dsp;

QHash <QString, QString> global_palette;


CDocumentHolder *documents;
CFxRackWindow *wnd_fxrack;


SRC_STATE *resampler;

#define ENVELOPE_SIDE 20

int meter_cps;
int meter_msecs_delay;

extern int buffer_size_frames;

bool play_l;
bool play_r;

bool bypass_mixer;

//global variable with file formats
extern CFileFormats *file_formats;

//sound clipboard
CSoundBuffer *sound_clipboard; //incapsulate it?

//another global variable, also used at the transport code in eko.cpp
int transport_state = 0;


//function to overwrite data in this soundbuffer from the other one,
//from the pos position
//no channels count or samplerate check
void CSoundBuffer::overwrite_at (CSoundBuffer *other, size_t pos)
{
  if (! other && ! other->buffer)
     return;

  size_t c = 0;
  size_t i = pos;
  
  while (c < other->samples_total)
        {
         buffer[i] = other->buffer[c++];
         i++;
        }
}


//convert this sb to stereo and return a new instance
CSoundBuffer* CSoundBuffer::convert_to_stereo (bool full)
{
  if (channels != 1)
     return NULL;

  size_t new_samples = frames * 2;
  size_t new_buffer_size = new_samples * sizeof (float);
  
  float *tb = new float [new_samples];

  size_t c = 0;
  size_t i = 0;
  
  if (! full)
  while (c < new_samples)
       {
        tb[c++] = buffer [i++] * 0.5;
        tb[c++] = buffer [i] * 0.5;
       }
  else     
  while (c < new_samples)
       {
        tb[c++] = buffer [i++];
        tb[c++] = buffer [i];
       }
  
  CSoundBuffer *nb = new CSoundBuffer;

  nb->buffer = tb;
  
  nb->buffer_offset = 0;
  nb->buffer_size = new_buffer_size;
  
  nb->channels = 2;
  nb->samples_total = new_samples;
  nb->frames = new_samples / 2;
  nb->samplerate = samplerate;  
  
  return nb;
}


//the same thing for the mono
//stereo channels are distributed to mono 
//with a half level from both channels
CSoundBuffer* CSoundBuffer::convert_to_mono()
{
  if (channels != 2)
     return NULL;
  
  CSoundBuffer *nb = new CSoundBuffer;
    
  size_t new_samples = frames + 1;
  size_t new_buffer_size = new_samples * sizeof (float);
 
  float *tb = new float [new_samples];

  size_t c = 0;
  size_t i = 0;
    
  while (i < samples_total)
       {
        float l = buffer [i] * 0.5;
        i++;

        float r = buffer [i] * 0.5;
        i++;

        tb[c++] = l + r;
       }

  nb->buffer_offset = 0;
  nb->buffer_size = new_buffer_size;
  
  nb->samples_total = new_samples;
  nb->frames = new_samples;
  
  nb->sndfile_format = sndfile_format;
  nb->buffer = tb;
  nb->samplerate = samplerate;  
  nb->channels = 1;
  
  return nb;
}


//convert this sb to stereo and return a new instance
CSoundBuffer* CSoundBuffer::convert_ch6_to_stereo (int algo)
{
  if (channels != 6)
     return NULL;

  int six_channels[5];

  if (((sndfile_format & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV) ||
      ((sndfile_format & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAVEX) ||
      ((sndfile_format & SF_FORMAT_TYPEMASK) == SF_FORMAT_FLAC)
     )
     {
      six_channels[CH_FRONT_LEFT] = 0;
      six_channels[CH_FRONT_RIGHT] = 1;
      six_channels[CH_FRONT_CENTER] = 2;
      six_channels[CH_LOW_FREQUENCY] = 3;
      six_channels[CH_BACK_LEFT] = 4;
      six_channels[CH_BACK_RIGHT] = 5;
     }

  if ((sndfile_format & SF_FORMAT_TYPEMASK) == SF_FORMAT_AIFF)
     {
      six_channels[CH_FRONT_LEFT] = 0;
      six_channels[CH_FRONT_RIGHT] = 3;
      six_channels[CH_FRONT_CENTER] = 2;
      six_channels[CH_LOW_FREQUENCY] = 5;
      six_channels[CH_BACK_LEFT] = 1;
      six_channels[CH_BACK_RIGHT] = 4;
     }


  size_t new_samples = frames * 2;
  size_t new_buffer_size = new_samples * sizeof (float);

  float *tb = new float [new_samples];

  size_t end = samples_total - 1;
  size_t i = 0;
  size_t c = 0;

  while (i < end)
       {

        float front_left = buffer[i + six_channels[CH_FRONT_LEFT]];
        float front_right = buffer[i + six_channels[CH_FRONT_RIGHT]];
        float front_center = buffer[i + six_channels[CH_FRONT_CENTER]];
        float back_left = buffer[i + six_channels[CH_BACK_LEFT]];
        float back_right = buffer[i + six_channels[CH_BACK_RIGHT]];
        float lfe = buffer[i + six_channels[CH_LOW_FREQUENCY]];

        float rear_gain = 0.5;
        float center_gain = 0.7;

        float left;
        float right;

        if (algo == 0)
           {
            left = front_left + rear_gain * back_left + center_gain * front_center;
            right = front_right + rear_gain * back_right + center_gain * front_center;
           }
        else
        if (algo == 1) //Dolby Surround-like
           {
            float tmp_mono_rear = 0.7 * (back_left + back_right);
            left = front_left + rear_gain * tmp_mono_rear + center_gain * front_center;
            right = front_right + rear_gain * tmp_mono_rear + center_gain * front_center;
           }


        tb[c++] = left;
        tb[c++] = right;
        i += 6;
       }

  CSoundBuffer *nb = new CSoundBuffer;

  nb->buffer = tb;

  nb->buffer_offset = 0;
  nb->buffer_size = new_buffer_size;

  nb->channels = 2;
  nb->samples_total = new_samples;
  nb->frames = new_samples / 2;
  nb->samplerate = samplerate;

  return nb;
}


/*

http://www.hydrogenaudio.org/forums/lofiversion/index.php/t44593.html

Some software decoders allow you to choose between two different 2-channel-downmix modes: Stereo and Dolby Surround:

Stereo:
- Left = front_left + rear_gain * rear_left + center_gain * center
- Right = front_right + rear_gain * rear_right + center_gain * center

Dolby Surround (Pro Logic Downmix)
- tmp_mono_rear = 0.7 * (rear_left + rear_right)
- Left = front_left + rear_gain * tmp_mono_rear + center_gain * center
- Right = front_right - rear_gain * tmp_mono_rear + center_gain * center

where rear_gain is usually around 0.5-1.0 and center_gain is almost always 0.7 (-3 dB) if I recall correctly. Notice the polarity inversion of the tmp_mono_rear when routing to the 'Right' channel. So, the rear channels are first mixed together and then fed to Left and Right with a phase difference of 180°. This way a Pro Logic Decoder can try to recover the (mono!) rear channel.

Somehow Dolby Pro Logic 2 manages to squeeze 2 rear channels into a stereo signal. In this case I'm not sure how you should downmix the signal for such a decoder -- probably like this:
- Left = front_left + rear_gain * rear_left + center_gain * center
- Right = front_right - rear_gain * rear_right + center_gain * center

*/


/*

Every file format has a different channel ordering. The following table gives this ordering for some formats (useful for plugin writers )
reference: channel 1: channel 2: channel 3: channel 4: channel 5: channel 6:
5.1 WAV front left channel front right channel front center channel LFE rear left channel rear right channel
5.1 AC3 front left channel front center channel front right channel rear left channel rear right channel LFE
5.1 DTS front center channel front left channel front right channel rear left channel rear right channel LFE
5.1 AAC front center channel front left channel front right channel rear left channel rear right channel LFE
5.1 AIFF front left channel rear left channel front center channel front right channel rear right channel LFE

* 5.1 DTS: the LFE is on a separate stream (much like on multichannel MPEG2).
* AAC specifications are unavailable on the internet (a free version
*/





//returns the resampled data as a new buffer
//the source data remains untouched
CSoundBuffer* CSoundBuffer::resample (int new_rate)
{
  SRC_DATA data;
 
  data.src_ratio = (float) 1.0 * new_rate / samplerate; 
  
  data.input_frames = frames;
  data.output_frames = (int) floor (frames * data.src_ratio) ;

  float *data_out = new float [data.output_frames * channels];
    
  data.data_in = buffer;
  data.data_out = data_out;
 
  int error = src_simple (&data, SRC_SINC_BEST_QUALITY, channels);
  if (error)
     {
      qDebug() << src_strerror (error);
      delete data_out;
      return NULL;
     }

  CSoundBuffer *nb = new CSoundBuffer;
  
  nb->sndfile_format = sndfile_format;
  nb->buffer = data_out;
  nb->buffer_size = data.output_frames * channels * sizeof (float);
  nb->frames = data.output_frames;
  nb->samples_total = data.output_frames * channels;
  nb->samplerate = new_rate;  
  nb->channels = channels;
  
  return nb;
}


//delete data at the range from start to end, in samples
void CSoundBuffer::delete_range (size_t sample_start, size_t sample_end)
{
  if (! buffer)
     return;

  size_t range = sample_end - sample_start;
  
  size_t new_buffer_samples_count = samples_total - range;
  size_t new_buffer_size = buffer_size - (range * sizeof (float));

  float *dest_buffer = new float [new_buffer_samples_count];
           
  float *pdest = dest_buffer;
  float *psource = buffer;

  memcpy (dest_buffer, psource, sample_start * sizeof (float));
  
  dest_buffer += sample_start;
  psource += sample_start;
  psource += range;
  
  memcpy (dest_buffer, psource, buffer_size - 
         (sample_start * sizeof(float)) - 
          range * sizeof (float) - 2);
  
  if (buffer)
     delete buffer;
  
  buffer = pdest;
  buffer_size = new_buffer_size;
  frames = new_buffer_samples_count / channels;
  samples_total = new_buffer_samples_count;
}


//copy parameters from "sb" to this sound buffer
void CSoundBuffer::copy_params (CSoundBuffer *sb)
{
  if (! sb)
      return;

  buffer_offset = sb->buffer_offset;
  buffer_size = sb->buffer_size;
  
  channels = sb->channels;
  samples_total = sb->samples_total;
  
  frames = sb->frames;
  samplerate = sb->samplerate;
  
  use_sndfile_format = sb->use_sndfile_format;
  sndfile_format = sb->sndfile_format;
}


//copy all data from "other" to this buffer, including the data parameters
void CSoundBuffer::copy_from (CSoundBuffer *other)
{
  if (! other || ! other->buffer)
    return;

  copy_params (other);

  if (buffer)
     delete buffer; 

  buffer = new float [samples_total];

  memcpy (buffer, other->buffer, buffer_size); 
}


void CSoundBuffer::copy_from_w_resample (CSoundBuffer *other)
{
  if (! other || ! other->buffer)
    return;

  int f = sndfile_format;

  if (samplerate != other->samplerate) //TEST IT!
     {
      CSoundBuffer *sb = other->resample (samplerate);
      copy_from (sb);
      sndfile_format = f;
      delete sb;
      return;
     }

   copy_from (other);
   sndfile_format = f;
}


//paste from "other" to this buffer, at the "pos" position
//resample if it needs to be resampled
void CSoundBuffer::paste_at (CSoundBuffer *other, size_t pos)
{
  if (! other && ! other->buffer)
     return;

  if (samplerate != other->samplerate) //TEST IT!
     {
      CSoundBuffer *sb = other->resample (samplerate);
      paste_at (sb, pos);
      delete sb;
      return;
     }

  CSoundBuffer *temp_buffer = NULL;

  if (channels == 1 && other->channels == 2)
     temp_buffer = other->convert_to_mono();
  else
  if (channels == 2 && other->channels == 1)
      temp_buffer = other->convert_to_stereo (false);
  else
  if (channels == other->channels)
     {
      temp_buffer = new CSoundBuffer;
      temp_buffer->copy_from (other);
     }


  size_t position_in_samples = pos;

  size_t new_buffer_samples_count = temp_buffer->samples_total + samples_total;
  size_t new_buffer_size = temp_buffer->buffer_size + buffer_size;

  float *dest_buffer = new float [new_buffer_samples_count];

  float *pdest = dest_buffer;
  float *psource = buffer;

  memcpy (dest_buffer, psource, position_in_samples * sizeof (float));


  dest_buffer += position_in_samples;

  memcpy (dest_buffer, temp_buffer->buffer, temp_buffer->buffer_size);

  dest_buffer += temp_buffer->samples_total;

  psource += position_in_samples;

  memcpy (dest_buffer, psource, buffer_size - (position_in_samples * sizeof(float)));

  if (buffer)
     delete buffer;

  buffer = pdest;
  buffer_size = new_buffer_size;
  frames = new_buffer_samples_count / channels;

  samples_total = new_buffer_samples_count;
  delete temp_buffer;
}


CSoundBuffer::CSoundBuffer (QObject *parent): QObject (parent)
{
  buffer = 0;
  
  channels = 0;
  frames = 0;
  
  samples_total = 0;
  
  frames = 0;
  samplerate = 0;
  
  use_sndfile_format = true;
  sndfile_format = 0;
  
  sndfile_format = sndfile_format | SF_FORMAT_WAV | SF_FORMAT_FLOAT;
  
  buffer_offset = 0;
  buffer_size = 0;
}
 
 
CSoundBuffer::~CSoundBuffer()
{
  if (buffer)
     delete buffer;
     
  buffer = 0;
}


//copy data and its properties from this soundbuffer to new soundbuffer,
//in a given range, in samples
//return the new soundbuffer
CSoundBuffer* CSoundBuffer::copy_to (size_t sample_start, size_t sample_end)
{
  size_t limit = sample_end;
  
  if (sample_end > samples_total)
     limit = samples_total;

  size_t size_in_samples = limit - sample_start;

  size_t nbuffer_size = size_in_samples * sizeof (float);

  float *psource = buffer;
  psource += sample_start;

  CSoundBuffer *temp_buffer = new CSoundBuffer; 
         
  temp_buffer->buffer = new float [size_in_samples];
  temp_buffer->samples_total = size_in_samples;
  temp_buffer->buffer_size = nbuffer_size;
   
  temp_buffer->frames = size_in_samples / channels;
  temp_buffer->samplerate = samplerate;
  temp_buffer->channels = channels;
  temp_buffer->sndfile_format = sndfile_format;
    
  memcpy (temp_buffer->buffer, psource, nbuffer_size);

  return temp_buffer;
}


CUndoElement::CUndoElement()
{
  buffer = 0;
  type = UNDO_UNDEFINED;
  frames_per_section = 0;

  sample_cursor = 0;
  sample_start = 0; //sample
  sample_end = 0;  //sample
  
  selected = false;
}

 
CUndoElement::~CUndoElement()
{
  if (buffer)
     delete buffer;
}


CChannelSamples::CChannelSamples (size_t size)
{
  samples = new float [size];
  temp = 0.0;
}


CChannelSamples::~CChannelSamples()
{
  delete [] samples;
} 


CChannelMinmax::CChannelMinmax (size_t size)
{
  max_values = new CChannelSamples (size);
  min_values = new CChannelSamples (size);
}


CChannelMinmax::~CChannelMinmax()
{
  delete max_values;
  delete min_values;
}


CMinmaxes::CMinmaxes (size_t size, size_t sections)
{
  values = new CChannelMinmax* [size];
  count = size;
  for (size_t i = 0; i < count; i++)
      values[i] = new CChannelMinmax (sections);  
}


CMinmaxes::~CMinmaxes()
{
  for (size_t i = 0; i < count; i++)
      delete values[i];

  delete [] values;     
} 


CTimeRuler::CTimeRuler (QWidget *parent): QWidget (parent)
{
  setMinimumHeight (1);
  setMaximumHeight (24);

  resize (width(), 24);
  init_state = true;
}


void CTimeRuler::paintEvent (QPaintEvent *event)
{
  if (init_state)
     {
      QWidget::paintEvent (event);  
      return; 
     }

  QImage img (width(), height(), QImage::Format_RGB32);

  QPainter painter (&img);

  painter.setFont (QFont ("Mono", 6));

  painter.setBrush (background_color);
  painter.drawRect (0, 0, width(), height());

  if (! waveform && ! waveform->sound_buffer->buffer)
     {
      qDebug() << "! waveform";
      event->accept();
      return;
     }
  
  size_t sections = waveform->section_to - waveform->section_from;
  float sections_per_second = (float) waveform->sound_buffer->samplerate / waveform->frames_per_section;
 
 // qDebug() << "frames_per_section = " << waveform->frames_per_section;
  //qDebug() << "sections = " << sections;
  //qDebug() << "sections_per_second = " << sections_per_second;

  //how many mseconds fits to the current window sections range?
  
 // if (sections_per_second == 0) //nasty hack
   //  sections_per_second = 1;

  size_t seconds_start = waveform->section_from / sections_per_second;
  size_t seconds_end = waveform->section_to / sections_per_second;
    
  size_t seconds = seconds_end - seconds_start;

  bool measure_in_seconds = true;
  
//  if (seconds > 60)
  if (seconds > 15)
     measure_in_seconds = false;  
        
  painter.setPen (foreground_color);
        
  for (size_t x = 0; x < sections; x++)
      {
      if (measure_in_seconds)
         {
          if (x % (int)ceil (sections_per_second) == 0)
            {
             QPoint p1 (x, 1);
             QPoint p2 (x, 12);

             QTime t (0, 0);

             t = t.addSecs ((waveform->section_from + x) / sections_per_second);       

             painter.drawLine (p1, p2);
             painter.drawText (x + 2, 20, t.toString ("ss:zzz") + "s");
            }
         }
      else
         if (x % 60 == 0)
            {
             QPoint p1 (x, 1);
             QPoint p2 (x, 12);

             QTime t (0, 0);

             t = t.addSecs ((waveform->section_from + x) / sections_per_second);       

             painter.drawLine (p1, p2);
             painter.drawText (x + 2, 20, t.toString ("mm:ss") + "m");
            }
      }        

  QPainter painter2 (this);
  painter2.drawImage (0, 0, img);

  event->accept();
}


void CWaveform::paintEvent (QPaintEvent *event)
{
  if (init_state)
     {
      QWidget::paintEvent (event);
      return;
     } 

  QPainter painter (this);
  painter.drawImage (0, 0, waveform_image);

  int section = get_cursor_position();

  //QPen pen (cl_cursor);
  //pen.setWidth (2);
  //painter.setPen (pen);

  painter.setPen (cl_cursor);

  
  int x = section - section_from;
  painter.drawLine (x, 1, x, height());

  if (selected)
  if (get_selection_start() != get_selection_end())
     {
      int s_start = get_selection_start() - section_from;  //section start
      int s_end = get_selection_end() - section_from;  //section end

      QPoint p1 (s_start, 0);
      QPoint p2 (s_end, height());
      QRect r (p1, p2);
      
      painter.fillRect (r, cl_waveform_selection_foreground);
     }
     
  event->accept();
}


void CWaveform::timer_timeout()
{
  
  if (play_looped) 
  if (sound_buffer->buffer_offset > sel_end_samples) 
     {
      qDebug() << "buffer_offset: " << sound_buffer->buffer_offset;
      qDebug() << "sel_end_samples: " << sel_end_samples;
      qDebug() << "diff: " << sound_buffer->buffer_offset - sel_end_samples;
     }
   
  if (get_cursor_position() > section_to) 
     {
      //if ((scrollbar->value() + width()) < (sections_total - width()))
      scrollbar->setValue (scrollbar->value() + width());
     }
  else 
      update();
      
  set_cursorpos_text();
}  


CWaveform::CWaveform (QWidget *parent): QWidget (parent)
{
  play_looped = false;
  show_db = true;

  previous_mouse_pos_x = 0;

  selection_selected = 0;

  init_state = true;
  scale_factor = 1.0;
  section_from = 0;
  section_to = width() - 1;
  

  max_undos = 3;
    
  setMouseTracking (true);
  mouse_pressed = false;
  normal_cursor_shape = true;
  selected = false;

  minmaxes = 0;
  
  sound_buffer = 0;

  sound_buffer = new CSoundBuffer;
  
  set_cursor_value (0);
  set_selstart_value (0);
  set_selend_value (0);

  connect(&timer, SIGNAL(timeout()), this, SLOT(timer_timeout()));
  timer.setInterval (50); //сделать зависимым от текущего масштаба! was 40
}


CWaveform::~CWaveform()
{
  timer.stop();
  
  if (sound_buffer)
     delete sound_buffer;
  
  if (minmaxes)
     delete minmaxes;

  flush_undos();
}


void CWaveform::flush_undos()
{
  foreach (CUndoElement *el, undos)
           delete el;
}


inline bool x_nearby (int x, int pos, int width)
{
  int a = pos - width;
  int b = pos + width; 

  if (x >= a && x <= b)
     return true;
     
  return false;
}


void CWaveform::deselect()
{ 
  selected = false;

  sel_start_samples = sound_buffer->buffer_offset;
  sel_end_samples = sound_buffer->buffer_offset;
  
  selection_selected = 0;
}


void CWaveform::recalc_view()
{
  //qDebug() << "CWaveform::recalc_view() - start";

  if (! sound_buffer)
     return;

  if (! sound_buffer->buffer)
     return;

  /*
  size_t cursor_frames = cursor_position * frames_per_section;

  size_t sel_start_frames = selection_start * frames_per_section;
  size_t sel_end_frames = selection_end * frames_per_section;

  qDebug() << "frames_per_section = " << frames_per_section;
  qDebug() << "sel_start_frames = " << sel_start_frames;
  qDebug() << "sel_end_frames = " << sel_end_frames;
  qDebug() << "cursor_position = " << cursor_position;
  qDebug() << "cursor_frames = " << cursor_frames;
*/
  sections_total = width() * scale_factor;

  frames_per_section = ceil (sound_buffer->frames / sections_total);
  

  //qDebug() << "sel_start_frames = " << sel_start_frames;
  //qDebug() << "sel_end_frames = " << sel_end_frames;
  //qDebug() << "frames_per_section = " << frames_per_section;

  //qDebug() << "cursor_frames = " << cursor_frames;

  scrollbar->setMinimum (0);
  scrollbar->setMaximum (sections_total - width());

  // << "CWaveform::recalc_view() - end";

}



void CWaveform::scale (int delta)
{
 //qDebug() << "CWaveform::scale - start";

 if (! sound_buffer->buffer)
     return;

  int old_section_from = section_from;
  int old_frame_from = old_section_from * frames_per_section;

  if (delta > 0)
     scale_factor += 0.1f;
  else    
     scale_factor -= 0.1f;
    
  if (scale_factor < 1.0f)
     scale_factor = 1.0f;

  if ((width() * scale_factor) == sound_buffer->frames - 1) //can be scale factor so large?
      return;

  recalc_view();
  prepare_image();
  
  int new_section = old_frame_from / frames_per_section;   
  scrollbar->setValue (new_section);

  update();   
  timeruler->update();

  //qDebug() << "CWaveform::scale - end";
}


void CWaveform::wheelEvent (QWheelEvent *event)
{
  scale (event->delta());
  event->accept();
}  


void CWaveform::resizeEvent (QResizeEvent *event)
{
  recalc_view();
  
  section_from = scrollbar->value();
  section_to = width() + scrollbar->value();
  
  prepare_image();
}


void CWaveform::keyPressEvent (QKeyEvent *event)
{
  if (! sound_buffer->buffer)
     {
      event->accept();
      return;
     } 


  if (event->key() == Qt::Key_Delete)
     {
      wave_edit->waveform->delete_selected();
      event->accept();
      return;
     }


  if (event->key() == Qt::Key_Space)
     {
      wave_edit->doc->holder->transport_control->call_play_pause();

      event->accept();   
      return;
     }
  

  if (event->key() == Qt::Key_Return)
     {
      wave_edit->doc->holder->transport_control->call_stop();
      event->accept();   
      return;
     }


  if (event->key() == Qt::Key_Home)
     {
      set_cursor_value (0);

      scrollbar->setValue (0);         
             
      if (event->modifiers() & Qt::ShiftModifier)  
         {
          set_selstart_value (0);
          selected = true;   
         }
     
      update();

      set_cursorpos_text();
     
      event->accept();   
      return;
     }
 
  
  if (event->key() == Qt::Key_End)
     {
      set_cursor_value (sections_total - 1);
  
      scrollbar->setValue (sections_total - width());         
         
      if (event->modifiers() & Qt::ShiftModifier)  
         {
          set_selend_value (sections_total - 1);
          selected = true;   
         }
     
       update();

       set_cursorpos_text();
     
       event->accept();   
       return;
      }


  if (event->key() == Qt::Key_Left)
     {
      if (get_cursor_position() != 0)
         set_cursor_by_section (get_cursor_position() - 1);

      if (get_cursor_position() == section_from)
         {
          if (scrollbar->value() != scrollbar->minimum())
             scrollbar->setValue (scrollbar->value() - 1);         
             
          event->accept();   
          return;
         }  

      if (event->modifiers() & Qt::AltModifier)  
         {
          if (get_selection_start() != 0)
             {
              set_selstart_value (get_selection_start() - 1);
              fix_selection_bounds();
             } 
              
          selected = true;
         }
         
       if (event->modifiers() & Qt::ShiftModifier)  
          {
          if (get_selection_start() != 0)
              {
               set_selend_value (get_selection_end() - 1);
               fix_selection_bounds();
              } 
              
           selected = true;   
          }
       
     
      update();

      set_cursorpos_text();
     
      event->accept();   
      return;
     }
 

  if (event->key() == Qt::Key_Right)
     {
      set_cursor_by_section (get_cursor_position() + 1);

      if (get_cursor_position() == sections_total)
         set_cursor_by_section (get_cursor_position() - 1);
  
      if (get_cursor_position() == section_to)
         {
          if (scrollbar->value() != scrollbar->maximum())
             scrollbar->setValue (scrollbar->value() + 1);         
             
          event->accept();
          return;
         }  
         
        
       if (event->modifiers() & Qt::AltModifier)  
         {
          if (get_selection_start() != 0)
             {
              set_selstart_value (get_selection_start() + 1);
              fix_selection_bounds();
             } 
              
          selected = true;
         }

         
       if (event->modifiers() & Qt::ShiftModifier)  
          {
           if (get_selection_end() != sections_total)
              {
               set_selend_value (get_selection_end() + 1);
               fix_selection_bounds();
              } 
              
           selected = true;   
          }
     
       update();

       set_cursorpos_text();
     
       event->accept();   
       return;
      }


  if (event->key() == Qt::Key_Plus)
     {
      scale (1);
      event->accept();   
      return;
     }
  

  if (event->key() == Qt::Key_Minus)
     {
      scale (-1);
      event->accept();   
      return;
     }
  

  if (event->key() == Qt::Key_Delete)
     {
      delete_selected();
      event->accept();
      set_cursorpos_text();
      return;
     }
 

  if (event->text() == "[")
      {
       set_selstart_value (get_cursor_position());
       fix_selection_bounds();

       selected = true;
       update();
       
       event->accept();   
       return;
      }
   

  if (event->text() == "]")
      {
       set_selend_value (get_cursor_position());

       fix_selection_bounds();

       selected = true;
       update();
             
       event->accept();   
       return;
      }
 
  QWidget::keyPressEvent (event);
}    



size_t CWaveform::sample_start()
{
  if (! selected)
     return 0;

  return sel_start_samples;
}


size_t CWaveform::sample_end()
{
  if (! selected)
     return sound_buffer->samples_total - 1;

  return sel_end_samples;
}


void CWaveform::copy_selected()
{
  if (! selected)
     return;

  if (sound_clipboard)
     delete sound_clipboard;
     
  sound_clipboard = sound_buffer->copy_to (sample_start(), sample_end());
}


void CWaveform::paste()
{
  if (! sound_clipboard && ! sound_clipboard->buffer)
     {
      qDebug() << "no sound data in clipboard"; 
      return;
     } 
  
  if (! sound_buffer->buffer)
     {
      sound_buffer->copy_from (sound_clipboard);
      
      recalc_view();
      prepare_image();

      init_state = false;
      timeruler->init_state = false;
    
      timeruler->update();
      update();
  
      return;
     }

  undo_take_shot (UNDO_PASTE);

  sound_buffer->paste_at (sound_clipboard, sound_buffer->buffer_offset);
  deselect();
      
  magic_update();
}



void CWaveform::cut_selected()
{
  if (! selected)
     return;

  copy_selected();
  
  undo_take_shot (UNDO_DELETE);
  
  delete_selected();
}


void CWaveform::magic_update()
{
  recalc_view();
  prepare_image();
  timeruler->update();
  update();
}


void CWaveform::load_color (const QString &fname)
{
  QHash <QString, QString> h = hash_load_keyval (fname);
  
  cl_waveform_background = QColor (h.value ("waveform_background", "white")); 
  cl_waveform_foreground = QColor (h.value ("waveform_foreground", "darkMagenta"));  
  cl_waveform_selection_foreground = QColor (h.value ("waveform_selection_foreground", "green"));  
  waveform_selection_alpha = h.value ("waveform_selection_alpha", "127").toInt();
  cl_waveform_selection_foreground.setAlpha (waveform_selection_alpha);
  cl_axis = QColor (h.value ("axis", "black"));  
  cl_cursor = QColor (h.value ("cursor", "red"));  
  cl_envelope = QColor (h.value ("envelope", "yellow"));  
  cl_text = QColor (h.value ("text", "black"));  
  cl_env_point_selected = QColor (h.value ("env_point_selected", "red"));  
  cl_shadow = QColor (h.value ("shadow_color", "grey"));  
  draw_shadow = h.value ("draw_shadow", "0").toInt();
  timeruler->background_color = QColor (h.value ("timeruler.background_color", cl_waveform_background.name())); 
  timeruler->foreground_color = QColor (h.value ("timeruler.foreground", cl_waveform_foreground.name())); 

  magic_update();
}
    

void CWaveform::fix_selection_bounds()
{
 if (selection_selected == 2 && (sel_start_samples > sel_end_samples))
    {
     size_t t = sel_start_samples;
     sel_start_samples = sel_end_samples;
     sel_end_samples = t;
     selection_selected = 1;
    }
  

size_t buffer_size_samples = buffer_size_frames * sound_buffer->channels;
    
  if (selection_selected == 2)  
     {
      size_t i = sel_start_samples;
  
      while (i <= sel_end_samples)
            {
             i += buffer_size_samples;
            }
        
      sel_end_samples = i - buffer_size_samples;
     }
  else    
  if (selection_selected == 1)  
     {
      size_t i = sel_end_samples;
  
      while (i >= sel_start_samples)
            {
             i -= buffer_size_samples;
            }
        
      sel_start_samples = i + buffer_size_samples;
     }
  



/*

  size_t diff = sel_end_samples - sel_start_samples;
  if (diff < buffer_size_samples)
     {
      sel_end_samples = buffer_size_samples;
      return; 
     }
               
  */
}


void CWaveform::fix_selection_bounds_release()
{
 
  //round to
  
  
  size_t buffer_size_samples = buffer_size_frames * sound_buffer->channels;
    
  if (selection_selected == 2)  
     {
      size_t i = sel_start_samples;
  
      while (i <= sel_end_samples)
            {
             i += buffer_size_samples;
            }
        
      sel_end_samples = i - buffer_size_samples;
     }
  else    
  if (selection_selected == 1)  
     {
      size_t i = sel_end_samples;
  
      while (i >= sel_start_samples)
            {
             i -= buffer_size_samples;
            }
        
      sel_start_samples = i + buffer_size_samples;
     }
  

/*

  size_t diff = sel_end_samples - sel_start_samples;
  if (diff < buffer_size_samples)
     {
      sel_end_samples = buffer_size_samples;
      return; 
     }
               
  */
}



void CWaveform::select_all()
{
  sel_start_samples = 0;
  sel_end_samples = sound_buffer->samples_total - 1;

  selected = true;
  update();
}


void CWaveform::undo_take_shot (int type, int param)
{
  CUndoElement *el = new CUndoElement;
  if (undos.count() == max_undos)
    {
     delete undos.at (undos.count() - 1);  
     undos.removeAt (undos.count() - 1);
    } 
        
  el->type = type;
  el->selected = selected;
  
  el->sample_start = sample_start();
  el->sample_end = sample_end();

  el->sample_cursor = sound_buffer->buffer_offset;
  el->frames_per_section = frames_per_section;

  el->sndfile_format = sound_buffer->sndfile_format;
  el->use_sndfile_format = sound_buffer->use_sndfile_format;
  el->channels = sound_buffer->channels;
  el->samplerate = sound_buffer->samplerate;
  
  if (type == UNDO_WHOLE)
     el->buffer = sound_buffer->copy_to (0, sound_buffer->samples_total - 1);

  if (type == UNDO_DELETE)
     el->buffer = sound_buffer->copy_to (sample_start(), sample_end());

  if (type == UNDO_MODIFY)
     el->buffer = sound_buffer->copy_to (sample_start(), sample_end());

  if (type == UNDO_PASTE)
     {
      el->sample_start = sound_buffer->buffer_offset;
      el->sample_end = el->sample_start + sound_clipboard->samples_total - 1;
     }

  if (type == UNDO_INSERT)
     {
      el->sample_start = sound_buffer->buffer_offset;
      el->sample_end = param;
     }

  undos.prepend (el);
} 


void CWaveform::delete_selected()
{
  if (! selected)
     return;

  if (! sound_clipboard && ! sound_clipboard->buffer)
     {
      qDebug() << "no sound data in clipboard"; 
      return;
     } 

  undo_take_shot (UNDO_DELETE);
  
  sound_buffer->delete_range (sample_start(), sample_end());
    
  deselect();
  magic_update();
}


void CWaveform::redo()
{
  
}

/*
void CWaveform::undo_top()
{
  if (undos.count() == 0)
     return;
    
  CUndoElement *el = undos.at (0);
 
  if (el->type == UNDO_UNDEFINED) 
     return;

  if (el->type == UNDO_WHOLE) 
     {
      delete sound_buffer;
      sound_buffer = el->buffer->copy_to (0, el->buffer->samples_total - 1);
     }  

  if (el->type == UNDO_DELETE) 
      sound_buffer->paste_at (el->buffer, el->sample_start);

  if (el->type == UNDO_PASTE) 
     sound_buffer->delete_range (el->sample_start, el->sample_end);
  
  if (el->type == UNDO_INSERT) 
     sound_buffer->delete_range (el->sample_start, el->sample_end);
    
  if (el->type == UNDO_MODIFY) 
     sound_buffer->overwrite_at (el->buffer, el->sample_start);
  
  recalc_view();

  selected = false;
  
  sel_start_samples = 0;
  sel_end_samples = 0;
 
  if (sound_buffer)
     set_cursor_value (el->sample_cursor * frames_per_section * sound_buffer->channels);

  prepare_image();
  timeruler->update();
  update();

  undos.removeAt (0);
  delete el;
}
*/


void CWaveform::undo_top()
{
  if (undos.count() == 0)
     return;
    
  CUndoElement *el = undos.at (0);
 
  if (el->type == UNDO_UNDEFINED) 
     return;

  if (el->type == UNDO_WHOLE) 
     {
      delete sound_buffer;
      sound_buffer = el->buffer->copy_to (0, el->buffer->samples_total - 1);
     }  

  if (el->type == UNDO_DELETE) 
      sound_buffer->paste_at (el->buffer, el->sample_start);

  if (el->type == UNDO_PASTE) 
     sound_buffer->delete_range (el->sample_start, el->sample_end);
  
  if (el->type == UNDO_INSERT) 
     sound_buffer->delete_range (el->sample_start, el->sample_end);
    
  if (el->type == UNDO_MODIFY) 
     sound_buffer->overwrite_at (el->buffer, el->sample_start);
  
  recalc_view();

  selected = el->selected;
  
  sel_start_samples = el->sample_start;
  sel_end_samples = el->sample_end;
 
  if (sound_buffer)
     //set_cursor_value (el->sample_cursor * frames_per_section * sound_buffer->channels);
     sound_buffer->buffer_offset = el->sample_cursor;

  prepare_image();
  timeruler->update();
  update();

  undos.removeAt (0);
  delete el;
}


void CWaveform::set_cursor_to_sample (size_t sampleno)
{
  if (sampleno <= sound_buffer->buffer_size)
      sound_buffer->buffer_offset = sampleno;
}


void CWaveform::set_statusbar_text()
{
  //qDebug() << "CWaveform::set_statusbar_text()";

  if (frames_per_section == 0)
     return;

  QString txt;

  int framesstart = sample_start() / sound_buffer->channels;
  int framesend = sample_end() / sound_buffer->channels;

  //qDebug() << "framesstart: " << framesstart; 
  //qDebug() << "framesend: " << framesstart; 

  float msecs_selstart = (float) framesstart / sound_buffer->samplerate * 1000;
  float msecs_selend = (float) framesend / sound_buffer->samplerate * 1000;
  float diff = msecs_selend - msecs_selstart;
  
  //qDebug() << "diff: " << diff;
  
  QTime a (0, 0);
  a = a.addMSecs ((int) msecs_selstart);       

  QTime b (0, 0);
  b = b.addMSecs ((int) msecs_selend);       

  QTime d (0, 0);
  d = d.addMSecs ((int) diff);       

  txt = a.toString ("hh:mm:ss.zzz");  
  txt.append (" <-> ");
  txt.append (b.toString ("hh:mm:ss.zzz"));     
  txt.append (" = ");
  txt.append (d.toString ("hh:mm:ss.zzz"));     
    
  wave_edit->doc->holder->l_status_bar->setText (txt);
  //documents->l_status_bar->setText (txt);
}

void CWaveform::set_cursorpos_text()
{
 // qDebug() << "CWaveform::set_cursorpos_text()";

  //size_t frames = get_cursor_position() * frames_per_section;
  
  size_t frames = sound_buffer->buffer_offset / sound_buffer->channels;
  
  float mseconds = (float) frames / sound_buffer->samplerate * 1000;
  
  QTime t (0, 0);
  t = t.addMSecs ((int)mseconds);       
  wave_edit->doc->holder->l_maintime->setText (t.toString ("hh:mm:ss.zzz"));
  //documents->l_maintime->setText (t.toString ("hh:mm:ss.zzz"));
}


//MOVE TO CWAVEFORM?
void CWaveEdit::dragEnterEvent (QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat ("text/uri-list"))
      event->acceptProposedAction();
}


CWaveEdit::CWaveEdit (QWidget *parent): QWidget (parent)
{
  QVBoxLayout *vlayout = new QVBoxLayout();

  waveform = new CWaveform (0);

  waveform->wave_edit = this;

  scb_horizontal = new QScrollBar (Qt::Horizontal);
  waveform->scrollbar = scb_horizontal;

  timeruler = new CTimeRuler (0);
  timeruler->waveform = waveform;
  waveform->timeruler = timeruler;

  vlayout->addWidget (timeruler);
  vlayout->addWidget (waveform);
  vlayout->addWidget (scb_horizontal);
  
  setLayout (vlayout);
  connect (scb_horizontal, SIGNAL(valueChanged(int)), this, SLOT(scb_horizontal_valueChanged(int)));
}


//PUT IT TO THE CWAVEFORM?
void CWaveEdit::dropEvent (QDropEvent *event)
{
  QString fName;
  QFileInfo info;

  if (! event->mimeData()->hasUrls())
     return;
  
  foreach (QUrl u, event->mimeData()->urls())
         {
          fName = u.toLocalFile();
          info.setFile (fName);
          if (info.isFile())
             doc->holder->open_file (fName);
             //documents->open_file (fName);
         }

  event->acceptProposedAction();
}
    

CWaveEdit::~CWaveEdit()
{
  delete waveform;
  delete timeruler;
}


bool CWaveEdit::isReadOnly()
{
  return false;
}


void CWaveEdit::scb_horizontal_valueChanged (int value)
{
  waveform->section_from = value;
  waveform->section_to = waveform->width() + value;
  
  waveform->init_state = false;
  timeruler->init_state = false;
  
  waveform->prepare_image();
  waveform->update();
  timeruler->update();
}  


void CDocument::update_title (bool fullname)
{
  QMainWindow *w = qobject_cast<QMainWindow *>(holder->parent_wnd);
  //QMainWindow *w = qobject_cast<QMainWindow *>(documents->parent_wnd);

  if (fullname)
     w->setWindowTitle (file_name);
  else
     w->setWindowTitle (QFileInfo (file_name).fileName());
  
  wave_edit->waveform->set_statusbar_text(); 
}


void CDocument::reload()
{
  if (file_exists (file_name))
      open_file (file_name);
}


bool CDocument::open_file (const QString &fileName, bool set_fname)
{
  //qDebug() << "CDocument::open_file - start";

/*  CTio *tio = holder->tio_handler.get_for_fname (fileName);

  if (! tio)
     {
      holder->log->log (tr ("file type of %1 is not supported")
                           .arg (fileName));
      return false;
     }
  
  QTime tm;
  tm.start();
  
  float *ff = tio->load (fileName);
  
  if (! ff)
     {
      holder->log->log (tr ("cannot open %1 because of: %2")
                           .arg (fileName)
                           .arg (tio->error_string));
      return false;
     }

  if (wave_edit->waveform->sound_buffer->buffer)
     free (wave_edit->waveform->sound_buffer->buffer);

  wave_edit->waveform->sound_buffer->buffer_size = tio->total_samples * sizeof (float);
  wave_edit->waveform->sound_buffer->samples_total = tio->total_samples;
  wave_edit->waveform->sound_buffer->frames = tio->frames;
  wave_edit->waveform->sound_buffer->samplerate = tio->samplerate;
  wave_edit->waveform->sound_buffer->channels = tio->channels;
  wave_edit->waveform->sound_buffer->sndfile_format = tio->format;
  wave_edit->waveform->sound_buffer->buffer = ff;
    
  ronly = tio->ronly;  
    
  if (set_fname)
     {
      file_name = fileName;
      set_tab_caption (QFileInfo (file_name).fileName());
      holder->log->log (tr ("%1 is open").arg (file_name));
     }

  QMutableListIterator <QString> i (holder->recent_files);
  
  while (i.hasNext())
        {
         if (i.next() == file_name) 
            i.remove();
        }

///////////////////set up the scrollbar

  wave_edit->waveform->init_state = false;
  wave_edit->timeruler->init_state = false;

  wave_edit->waveform->magic_update();

  int elapsed = tm.elapsed();

  holder->log->log (tr ("elapsed: %1 ms").arg (elapsed));
  
  //qDebug() << "CDocument::open_file - end";
*/
  return true;
}


bool CDocument::save_with_name (const QString &fileName)
{
 // qDebug() << "CDocument::save_with_name";
/*
  if (ronly)
     {
      holder->log->log (tr ("cannot save %1 because of: %2")
                           .arg (fileName)
                           .arg ("read-only format"));
      return false;                     
      
     }

  int fmt = wave_edit->waveform->sound_buffer->sndfile_format & SF_FORMAT_TYPEMASK;  
  QString fname = fileName;
  QString ext = file_get_ext (fileName);
  QString fext = file_formats->hextensions.value (fmt);
  
  if (ext.isEmpty())
     fname = fname + "." + fext;
  else
      if (ext != fext)
          fname = change_file_ext (fname, fext);

  CTio *tio = holder->tio_handler.get_for_fname (fname);
  
  if (! tio)
     return false;

  tio->input_data = wave_edit->waveform->sound_buffer->buffer;
  tio->frames = wave_edit->waveform->sound_buffer->frames;
  
  tio->samplerate = wave_edit->waveform->sound_buffer->samplerate;
  tio->channels = wave_edit->waveform->sound_buffer->channels;
  tio->format = wave_edit->waveform->sound_buffer->sndfile_format;

  QTime tm;
  tm.start();
  
  if (! tio->save (fname))
     {
      holder->log->log (tr ("cannot save %1 because of: %2")
      //documents->log->log (tr ("cannot save %1 because of: %2")
      
                           .arg (fname)
                           .arg (tio->error_string));
      return false;
     }

  int elapsed = tm.elapsed();

  file_name = fname;

  set_tab_caption (QFileInfo (file_name).fileName());

  holder->log->log (tr ("%1 is saved").arg (file_name));
  holder->log->log (tr ("elapsed: %1 milliseconds").arg (elapsed));
  
  //documents->log->log (tr ("%1 is saved").arg (file_name));


  update_title();
  //update_status();

 // textEdit->document()->setModified (false);
 */
  return true;
}


CDocument::CDocument (QObject *parent): QObject (parent)
{
  QString fname = tr ("new[%1]").arg (QTime::currentTime().toString ("hh-mm-ss"));
  
  ronly = false;
  file_name = fname;
  position = 0;
}


CDocument::~CDocument()
{
  /*if (textEdit->document()->isModified() && file_exists (file_name))
     {
      if (QMessageBox::warning (0, "EKO",
                                tr ("%1 has been modified.\n"
                                "Do you want to save your changes?")
                                .arg (file_name),
                                QMessageBox::Ok | QMessageBox::Default,
                                QMessageBox::Cancel | QMessageBox::Escape) == QMessageBox::Ok)
          save_with_name (file_name, charset);
     }
*/

 holder->add_to_recent (this);
 holder->update_recent_menu();
 
 delete wave_edit;

 int i = holder->tab_widget->indexOf (tab_page);
 if (i != -1)
     holder->tab_widget->removeTab (i);
}


void CDocument::create_new()
{
  wave_edit = new CWaveEdit;

  wave_edit->doc = this;
  wave_edit->waveform->load_color (holder->def_palette);
  
  int sndfile_format = 0;
  sndfile_format = sndfile_format | SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  int t = settings->value ("def_sndfile_format", sndfile_format).toInt();

  wave_edit->waveform->sound_buffer->sndfile_format = t;
  wave_edit->waveform->sound_buffer->channels = settings->value ("def_channels", 1).toInt();
  wave_edit->waveform->sound_buffer->samplerate = settings->value ("def_samplerate", 44100).toInt();

  int tab_index = holder->tab_widget->addTab (wave_edit, file_name);
  

  tab_page = holder->tab_widget->widget (tab_index);
  
  wave_edit->waveform->setFocus (Qt::OtherFocusReason);
  wave_edit->waveform->set_cursorpos_text();
}


int CDocument::get_tab_idx()
{
  return holder->tab_widget->indexOf (tab_page);
}


void CDocument::set_tab_caption (const QString &fileName)
{
  holder->tab_widget->setTabText (get_tab_idx(), fileName);
//  documents->tab_widget->setTabText (get_tab_idx(), fileName);

}


bool CDocument::save_with_name_plain (const QString &fileName)
{
  /*CTio *tio = holder->tio_handler.get_for_fname (fileName);

  tio->input_data = wave_edit->waveform->sound_buffer->buffer;
  tio->frames = wave_edit->waveform->sound_buffer->frames;
  tio->samplerate = wave_edit->waveform->sound_buffer->samplerate;
  tio->channels = wave_edit->waveform->sound_buffer->channels;
  tio->format = wave_edit->waveform->sound_buffer->sndfile_format;
  
  if (! tio->save (fileName))
     {
      holder->log->log (tr ("cannot save %1 because of: %2")
                           .arg (fileName)
                           .arg (tio->error_string));
      return false;
     }
*/
  return true;
}


void CDocument::goto_pos (size_t pos)
{

}


void CDocumentHolder::reload_recent_list (void)
{
  if (! file_exists (recent_list_fname))
     return;

  recent_files = qstring_load (recent_list_fname).split ("\n");
}


CDocumentHolder::~CDocumentHolder()
{
  if (sound_clipboard)
     delete sound_clipboard;

  if (! list.isEmpty())
  while (! list.isEmpty())
        delete list.takeFirst();

  qstring_save (recent_list_fname, recent_files.join ("\n"));
}


CDocument* CDocumentHolder::create_new()
{
  CDocument *doc = new CDocument;

  doc->holder = this;
  list.append (doc);

  doc->create_new();

  current = doc;

  tab_widget->setCurrentIndex (tab_widget->indexOf (doc->tab_page));
  apply_settings_single (doc);

  doc->update_title();
  //doc->update_status();

  return doc;
}


CDocument* CDocumentHolder::open_file (const QString &fileName, bool set_fname)
{
/*
  if (! file_exists (fileName))
     return NULL;

  if (! tio_handler.is_ext_supported (fileName))
     return NULL;


  foreach (CDocument *d, list)  
          if (d->file_name == fileName)
             {
              tab_widget->setCurrentIndex (tab_widget->indexOf (d->tab_page));
              return d;
             }

  CDocument *doc = create_new();

  doc->open_file (fileName, set_fname);
  doc->update_title();

  tab_widget->setCurrentIndex (tab_widget->indexOf (doc->tab_page));

  return doc;*/
}


void CDocumentHolder::close_current()
{
  int i = tab_widget->currentIndex();
  if (i < 0)
     return;

  CDocument *d = list[i];
  
  current = d;

  list.removeAt (i);
  delete d;
  
  current = 0;
}


CDocument* CDocumentHolder::get_current()
{
  int i = tab_widget->currentIndex();
  if (i < 0)
     {
      current = 0;
      return NULL;
     } 

  current = list[i];
  return current;
}


void CDocumentHolder::apply_settings_single (CDocument *d)
{
  d->wave_edit->waveform->show_db = settings->value ("meterbar_show_db", true).toBool();
  d->wave_edit->waveform->max_undos = settings->value ("max_undos", 6).toInt();
  d->wave_edit->waveform->prepare_image();
  d->wave_edit->waveform->update();
}


void CDocumentHolder::apply_settings()
{
  foreach (CDocument *d, list)
          apply_settings_single (d);
}


void CDocumentHolder::add_to_recent (CDocument *d)
{
  if (! file_exists (d->file_name))
     return;

  recent_files.prepend (d->file_name);
  
  if (recent_files.size() > 13)
     recent_files.removeLast();
}


void CDocumentHolder::update_recent_menu()
{
  recent_menu->clear();
  create_menu_from_list (this, recent_menu, recent_files, SLOT(open_recent()));
}


void CDocumentHolder::open_recent()
{
  QAction *Act = qobject_cast<QAction *>(sender());

  int i = recent_files.indexOf (Act->text ());
  if (i == -1)
     return;

  open_file (recent_files[i]);
  update_recent_menu();
}


void CDocumentHolder::save_to_session (const QString &fileName)
{
  if (list.size() < 0)
     return;

  fname_current_session = fileName;
  QString l;

  foreach (CDocument *d, list)
          {
           l += d->file_name;
           l += "\n";
          }
  
  qstring_save (fileName, l.trimmed());
}


void CDocumentHolder::load_from_session (const QString &fileName)
{
  if (! file_exists (fileName))
     return;

  QStringList l = qstring_load (fileName).split("\n");
  int c = l.size();
  if (c < 0)
     return;

  foreach (QString t, l)  
          open_file (t);

  fname_current_session = fileName;
}


void CDocumentHolder::load_palette (const QString &fileName)
{
  def_palette = fileName;
  
  foreach (CDocument *d, list)
          d->wave_edit->waveform->load_color (fileName);
}


CDocumentHolder::CDocumentHolder (QObject *parent):  QObject (parent)
{
  sound_clipboard = new CSoundBuffer; 
  current = 0;
}
 

void CTransportControl::call_play_pause()
{
  emit play_pause();
}


void CTransportControl::call_stop()
{
  emit stop();
}


void CWaveform::set_cursor_value (size_t section)
{
  sound_buffer->buffer_offset = section * frames_per_section * sound_buffer->channels;
}


void CWaveform::set_selstart_value (size_t section)
{
  if (sound_buffer)
     sel_start_samples = section * frames_per_section * sound_buffer->channels;
}


void CWaveform::set_selend_value (size_t section)
{
  if (sound_buffer)
     sel_end_samples = section * frames_per_section * sound_buffer->channels;
}


void CFxRackWindow::tm_level_meter_timeout()
{
  if (! level_meter)
      return;   

  level_meter->update();
}


void CVLevelMeter::update_scale_image()
{
  QImage im (scale_width, height(), QImage::Format_RGB32);

  QPainter painter (&im);

  painter.fillRect (0, 0, scale_width, height(), Qt::white);

  painter.setPen (Qt::black);
  painter.setFont (QFont ("Mono", 6));
 
  int ten = get_value (height(), 5);  
                    
  int percentage = 105;
  float ff = 0.f;
  
  for (int y = 0; y < height(); y++)
      {
       if (! (y % ten))
          {
           percentage -= 5;
          
           ff = (1.0 / 100) * percentage;
                                            
           QPoint p1 (1, y);
           QPoint p2 (scale_width, y);
           painter.drawLine (p1, p2);
               
           painter.drawText (QPoint (1, y), QString::number (float2db (ff), 'g', 2)); //dB
          }
       }         
     
  img_bar = im;
}


void CVLevelMeter::resizeEvent(QResizeEvent *event)
{
  update_scale_image();
}


CVLevelMeter::CVLevelMeter (QWidget *parent)
{
  peak_l = 0;
  peak_r = 0;

  scale_width = 42;
  bars_width = 48;

  setMinimumWidth (scale_width + bars_width);

  resize (scale_width + bars_width, 256);
}


#define FALLOFF_COEF 0.05f

void CVLevelMeter::paintEvent (QPaintEvent *event)
{
  if (pl > peak_l)
     peak_l = pl;
  else   
      peak_l -= FALLOFF_COEF;
     
  if (pr > peak_r)
     peak_r = pr;
  else   
      peak_r -= FALLOFF_COEF;

  if (peak_l < -1 || peak_l > 1)
     peak_l = 0;

  if (peak_r < -1 || peak_r > 1)
     peak_r = 0;

  QPainter painter (this);   
   
  int h = height();
  
  painter.fillRect (scale_width, 0, width(), height(), Qt::white);
       
  int lenl = h - (int)(peak_l * 1.0f * h);
  int lenr = h - (int)(peak_r * 1.0f * h);
      
  QPoint ltop (scale_width, height());
  QPoint lbottom (scale_width + (width() - scale_width)  / 2, lenl);
  
  QPoint rtop (scale_width + (width() - scale_width) / 2, height());
  QPoint rbottom (width(), lenr);
  
  QRect l (ltop, lbottom);
  QRect r (rtop, rbottom);
    
  painter.fillRect (l, Qt::green);
  painter.fillRect (r, Qt::darkGreen);

  painter.drawImage (1, 1, img_bar);
 
  event->accept();  
}


CFxRackWindow::~CFxRackWindow()
{
  delete fx_rack;
}



void CFxRackWindow::cb_l_changed (int value)
{
  play_l = value; 
}


void CFxRackWindow::cb_r_changed (int value)
{
  play_r = value; 
}



void CFxRackWindow::dial_gain_valueChanged (int value)
{
  float f = 1.0f;
  if (value == 0)
     {
      dsp->gain = 1.0f;
      return;
     }

   f = db2lin (value);

   dsp->gain = f;
}


void CFxRackWindow::dial_pan_valueChanged (int value)
{
   
   //0 - left, 0.5 - middle, 1 - right
   
   if (value == 0)
      {
       dsp->pan = 0.5;
       return;
      }
   
   if (value < 0)
      {
       int v = (100 - value * -1);
       dsp->pan = get_fvalue (0.5f, v);
      }
   
   if (value > 0)
      {
       dsp->pan = get_fvalue (0.5f, value) + 0.5f;
      }
}


CFxRackWindow::CFxRackWindow()
{
  QHBoxLayout *h_mainbox = new QHBoxLayout;
  setLayout (h_mainbox);

  QVBoxLayout *v_box = new QVBoxLayout;
  
  h_mainbox->addLayout (v_box);
  
  level_meter = new CVLevelMeter (this);
  
  QPushButton *bt_apply = new QPushButton (tr ("Apply"));
  connect (bt_apply, SIGNAL(clicked()), this, SLOT(apply_fx()));

  fx_rack = new CFxRack;

  v_box->addWidget (fx_rack->inserts);
  v_box->addWidget (bt_apply);

  QVBoxLayout *v_meter = new QVBoxLayout;
  v_meter->addWidget (level_meter); 
  
  cb_l.setText ("L");
  cb_r.setText ("R");
  cb_l.setChecked (true);
  cb_r.setChecked (true);
  
  connect(&cb_l, SIGNAL(stateChanged (int )), this, SLOT(cb_l_changed (int )));
  connect(&cb_r, SIGNAL(stateChanged (int )), this, SLOT(cb_r_changed (int )));

  
  QHBoxLayout *h_lr = new QHBoxLayout;
  h_lr->addWidget (&cb_l);
  h_lr->addWidget (&cb_r);
  
  v_meter->addLayout (h_lr); 
  

  h_mainbox->addLayout (v_meter); 
  
  connect(&tm_level_meter, SIGNAL(timeout()), this, SLOT(tm_level_meter_timeout()));
  
  //tm_level_meter.setInterval (meter_msecs_delay);
  tm_level_meter.setInterval (60);


  QVBoxLayout *v_vol = new QVBoxLayout;
  QLabel *l_vol = new QLabel (tr ("Volume"));
  QDial *dial_volume = new QDial;
  dial_volume->setWrapping (false);
  connect (dial_volume, SIGNAL(valueChanged(int)), this, SLOT(dial_gain_valueChanged(int)));
  dial_volume->setRange (-26, 26);

  
  v_vol->addWidget (l_vol);
  v_vol->addWidget (dial_volume);
  

  QVBoxLayout *v_pan = new QVBoxLayout;
  QLabel *l_pan = new QLabel (tr ("Pan"));
  QDial *dial_pan = new QDial;
  dial_pan->setWrapping (false);
  connect (dial_pan, SIGNAL(valueChanged(int)), this, SLOT(dial_pan_valueChanged(int)));
  dial_pan->setRange (-100, 100);

  
  v_pan->addWidget (l_pan);
  v_pan->addWidget (dial_pan);
  
  QHBoxLayout *h_volpan = new QHBoxLayout;
  h_volpan->addLayout (v_vol);
  h_volpan->addLayout (v_pan);
    
  v_box->addLayout (h_volpan);


  setWindowTitle (tr ("Mixer"));
}


void CFxRackWindow::closeEvent (QCloseEvent *event)
{
  event->accept();
}



void CFxRackWindow::apply_fx()
{
  CDocument *d = documents->get_current();
  if (! d)
     return;

  dsp->process_whole_document (d);
  wnd_fxrack->fx_rack->bypass_all();
}


//process the whole file or the selection
bool CDSP::process_whole_document (CDocument *d)
{
  if (! d)
     return false;

//  qDebug() << "CDSP::process_whole_document";


//here we work with short buffer to process it and output to it
  
  d->wave_edit->waveform->undo_take_shot (UNDO_MODIFY);


  size_t portion_size = 4096 * d->wave_edit->waveform->sound_buffer->channels;

  //qDebug() << "portion_size =" << portion_size;

  size_t sample_start = d->wave_edit->waveform->sample_start();
  size_t sample_end = d->wave_edit->waveform->sample_end();

  size_t nsamples = d->wave_edit->waveform->sound_buffer->samples_total;

  float *buf = d->wave_edit->waveform->sound_buffer->buffer;

  size_t offset = 0;

//FIXIT!!! CHECK IF sample_end > sample_start

  if (d->wave_edit->waveform->selected)
     {
      nsamples = sample_end - sample_start;
      offset = sample_start;
     }

  buf += offset;

  //qDebug() << "offset = " << offset;
  //qDebug() << "nsamples = " << nsamples;

  ////////////call fx chain

  offset = 0;

  while (offset < nsamples)
        {
         //qDebug() << "offset = " << offset;

         size_t diff = nsamples - offset;
         if (diff < portion_size)
            portion_size = diff;

         for (int i = 0; i < wnd_fxrack->fx_rack->effects.count(); i++)
             {
              if (! wnd_fxrack->fx_rack->effects[i]->bypass)
                 {
                  wnd_fxrack->fx_rack->effects[i]->realtime = false;
                 
                  wnd_fxrack->fx_rack->effects[i]->channels = d->wave_edit->waveform->sound_buffer->channels;
                  wnd_fxrack->fx_rack->effects[i]->samplerate = d->wave_edit->waveform->sound_buffer->samplerate;
                  
                  wnd_fxrack->fx_rack->effects[i]->execute (buf, portion_size);
                 }
             }

         offset += portion_size;
         buf += portion_size;
  }


  for (int i = 0; i < wnd_fxrack->fx_rack->effects.count(); i++)
      {
       if (! wnd_fxrack->fx_rack->effects[i]->bypass)
           wnd_fxrack->fx_rack->effects[i]->bypass = true;
      }

///////////

//apply the hard limiter
/*
  for (size_t i = 0; i < nsamples; i++)
      {
       if (buf[i] > 1.0f)
          buf[i] = 1.0f;
       else
           if (buf[i] < -1.0f)
              buf[i] = -1.0f;
      }
*/
  d->wave_edit->waveform->magic_update();

  return true;
}


const QModelIndex CFxRack::index_from_name (const QString &name)
{
  QList <QStandardItem *> lst = model->findItems (name);
  if (lst.size() > 0)
     return model->indexFromItem (lst[0]);
  else
      return QModelIndex();
}


void CFxRack::tv_activated (const QModelIndex &index)
{
  emit fx_activated (index.data().toString());
  //qDebug() << index.data().toString();

  int i = index.row();
  if (i != -1)
     effects.at (i)->show_ui();
}


void CFxRack::add_entry (AFx *f, bool checked)
{
  QStandardItem *item = new QStandardItem (f->name);
  item->setCheckable (true);

  if (checked)
     item->setCheckState (Qt::Checked);

  item->setFlags (Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                  Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable |
                  Qt::ItemIsDropEnabled);

  model->appendRow (item);
}


void CFxRack::ins_entry (AFx *f)
{
 /* CDocument *d = documents->get_current();
  if (! d)
     return;
*/
  QStandardItem *item = new QStandardItem (f->name);
  item->setCheckable (true);
  item->setCheckState (Qt::Checked);

  item->setFlags (Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                  Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable |
                  Qt::ItemIsDropEnabled);


  int i = get_sel_index();
  if (i == -1)
     {
      model->appendRow (item);
//      effects.append (f->self_create (d->wave_edit->waveform->sound_buffer->samplerate, d->wave_edit->waveform->sound_buffer->channels));
      effects.append (f->self_create (44100, 2)); //def vals

     }
  else
      {
       model->insertRow (i, item);
//       effects.insert (i, f->self_create (d->wave_edit->waveform->sound_buffer->samplerate, d->wave_edit->waveform->sound_buffer->channels));
       effects.insert (i, f->self_create (44100, 2));

      }
}

//FIXME!!!
void CFxRack::bt_up_clicked()
{
/*  CDocument *d = documents->get_current();
  if (! d)
     return;
*/
  if (! tree_view->selectionModel()->hasSelection())
     return;

  QModelIndex index = tree_view->selectionModel()->currentIndex();
  int row = index.row();
  if (row == 0)
     return;

  QList<QStandardItem *> l = model->takeRow (row);

  //QStandardItem *item = model->takeItem (row);

  int new_row = row - 1;

  model->insertRow (new_row, l[0]);

  effects.swap (row, new_row);

  //qDebug() << "row = " << row << " new row = " << new_row;

   //qDebug() << "FX: start";

/*
  foreach (AFx *f, effects)
          {
           qDebug() << f->name;
          }
*/
 // qDebug() << "FX: end";
}


//FIXME!!!
void CFxRack::bt_down_clicked()
{

  if (! tree_view->selectionModel()->hasSelection())
     return;

  QModelIndex index = tree_view->selectionModel()->currentIndex();
  int row = index.row();
  if (row == model->rowCount() - 1)
     return;

  QList<QStandardItem *> l = model->takeRow (row);

//  qDebug() << "before: " << model->rowCount();

  int new_row = row + 1 ;

  model->insertRow (new_row, l[0]);

//  qDebug() << "after: " << model->rowCount();

  effects.swap (row, new_row);

//  qDebug() << "FX: start";
/*
  foreach (AFx *f, effects)
          {
           qDebug() << f->name;
          }
*/
  //qDebug() << "FX: end";

  //qDebug() << "row = " << row << " new row = " << new_row;
}


CFxRack::CFxRack (QObject *parent): QObject (parent)
{
  avail_fx = new CFxList;

  model = new QStandardItemModel (0, 1, parent);

  connect (model, SIGNAL(dataChanged (const QModelIndex &, const QModelIndex &)),
          this, SLOT(bypass_dataChanged (const QModelIndex &, const QModelIndex &)));


  tree_view = new CFxTreeView;
  tree_view->setHeaderHidden (true);
  //tree_view->rack = this;
  tree_view->setRootIsDecorated (false);
  tree_view->setModel (model);

  tree_view->setDragEnabled (true);

  //tree_view->header()->setResizeMode (QHeaderView::ResizeToContents);
  //tree_view->header()->setStretchLastSection (false);

  tree_view->setSelectionMode (QAbstractItemView::ExtendedSelection);
  tree_view->setSelectionBehavior (QAbstractItemView::SelectRows);

  connect (tree_view, SIGNAL(activated(const QModelIndex &)), this, SLOT(tv_activated(const QModelIndex &)));


  inserts = new QWidget;

  QVBoxLayout *v_box = new QVBoxLayout;
  inserts->setLayout (v_box);

  QPushButton *bt_add = new QPushButton ("+");
  QPushButton *bt_del = new QPushButton ("-");

  QToolButton *bt_up = new QToolButton;
  QToolButton *bt_down = new QToolButton;
  bt_up->setArrowType (Qt::UpArrow);
  bt_down->setArrowType (Qt::DownArrow);

  connect (bt_up, SIGNAL(clicked()), this, SLOT(bt_up_clicked()));
  connect (bt_down, SIGNAL(clicked()), this, SLOT(bt_down_clicked()));

  connect (bt_add, SIGNAL(clicked()), this, SLOT(add_fx()));
  connect (bt_del, SIGNAL(clicked()), this, SLOT(del_fx()));


  QHBoxLayout *h_box = new QHBoxLayout;
  h_box->addWidget (bt_add);
  h_box->addWidget (bt_del);
  h_box->addWidget (bt_up);
  h_box->addWidget (bt_down);

  v_box->addWidget (tree_view);
  v_box->addLayout (h_box);
}


void CFxRack::add_fx()
{
  CTextListWindow w (tr ("Select"), tr ("Available effects"));

  w.list->addItems (avail_fx->names());

  int result = w.exec();

  if (result != QDialog::Accepted)
      return;

  AFx *f = avail_fx->find_by_name (w.list->currentItem()->text());

  ins_entry (f);

  print_fx_list();
}


void CFxRack::del_fx()
{
  int i = get_sel_index();
  if (i == -1)
     return;

  QList<QStandardItem *> l = model->takeRow (i);
  delete l[0];

  AFx *f = effects.takeAt (i);
  if (f)
     delete f;

  print_fx_list();
}


CFxRack::~CFxRack()
{
  foreach (AFx *f, effects)
         {
          f->wnd_ui->close();
          delete f;
         }
 
  delete avail_fx;
}


QString CFxRack::get_sel_fname()
{
  if (! tree_view->selectionModel()->hasSelection())
      return QString();

  QModelIndex index = tree_view->selectionModel()->currentIndex();
  return index.data().toString();
}


const QModelIndex CFxRack::index_from_idx (int idx)
{
  QStandardItem *item = model->item (idx);
  if (item)
     return model->indexFromItem (item);
  else
      return QModelIndex();
}


int CFxRack::get_sel_index()
{
  if (! tree_view->selectionModel()->hasSelection())
     return -1;

  QModelIndex index = tree_view->selectionModel()->currentIndex();
  return index.row();
}


void CFxTreeView::mouseMoveEvent (QMouseEvent *event)
{
 /* if (! (event->buttons() & Qt::LeftButton))
     return;

  QStringList l = fman->get_sel_fnames();
  if (l.size() < 1)
     return;

  QDrag *drag = new QDrag (this);
  QMimeData *mimeData = new QMimeData;

  QList <QUrl> list;

  foreach (QString fn, l)
           list.append (QUrl::fromLocalFile (fn));

  mimeData->setUrls (list);
  drag->setMimeData (mimeData);

  if (drag->exec (Qt::CopyAction |
                  Qt::MoveAction |
                  Qt::LinkAction) == Qt::MoveAction)
     fman->refresh();
    */
  event->accept();
}


void CFxRack::print_fx_list()
{
  foreach (AFx *f, effects)
          qDebug() << f->name;
}


void CFxRack::bypass_dataChanged (const QModelIndex & topLeft, const QModelIndex & bottomRight)
{
  bool b = model->data (topLeft, Qt::CheckStateRole).toInt();

  //qDebug() << model->data (topLeft, Qt::DisplayRole).toString() << " = " << b;
  //qDebug() << "row = " << topLeft.row();

  effects[topLeft.row()]->bypass = ! b;

  //if (effects[topLeft.row()]->bypass)
    // qDebug() << "bypassed";
}


void CFxRack::bypass_all (bool mode)
{
  for (int row = 0; row < model->rowCount(); row++)
      {
       QStandardItem *si = model->item (row);
       if (mode)
          si->setCheckState (Qt::Unchecked);
       else
          si->setCheckState (Qt::Checked);
      }
}


void CFxRack::set_state_all (FxState state)
{
  foreach (AFx *f, effects)
          {
           f->set_state (state);
          }
}


size_t CDSP::process (size_t nframes)
{
  //qDebug() << "CDSP::process -- start";
  
  if (nframes == 0)
     return 0;

  if (transport_state == STATE_EXIT)
     return 0;

  if (! documents->current)
     return 0;

  //if (documents->current->wave_edit->waveform->sound_buffer->buffer_offset >= documents->current->wave_edit->waveform->sound_buffer->buffer_size)
//     return 0;
   
  maxl = 0.0f;
  maxr = 0.0f;

//here we work with short buffer to process it and output to it

  size_t nsamples = nframes * documents->current->wave_edit->waveform->sound_buffer->channels;
  size_t nsamples_full = nsamples;
  
//  qDebug() << "nsamples: " << nsamples;
  
  size_t tail = documents->current->wave_edit->waveform->sound_buffer->buffer_size - documents->current->wave_edit->waveform->sound_buffer->buffer_offset;
  tail = tail - nsamples; 
  
  if (tail < nsamples)
     nsamples = tail;
                
 
  float *psource = documents->current->wave_edit->waveform->sound_buffer->buffer + 
                   documents->current->wave_edit->waveform->sound_buffer->buffer_offset;

  
  memset (temp_buffer, 0, nsamples_full * sizeof (float));  
  
  //just copy
  memcpy (temp_buffer, psource, nsamples * sizeof (float));

    
////////////call fx chain


  if (! bypass_mixer && documents->current)
  for (int i = 0; i < wnd_fxrack->fx_rack->effects.count(); i++)
      {
       if (! wnd_fxrack->fx_rack->effects[i]->bypass)
          { 
           wnd_fxrack->fx_rack->effects[i]->realtime = true;
           wnd_fxrack->fx_rack->effects[i]->channels = documents->current->wave_edit->waveform->sound_buffer->channels;
           wnd_fxrack->fx_rack->effects[i]->samplerate = documents->current->wave_edit->waveform->sound_buffer->samplerate;
           wnd_fxrack->fx_rack->effects[i]->execute (temp_buffer, nsamples);
          }
      }


//apply the hard limiter
      
      /*
  for (size_t i = 0; i < nsamples; i++)
      {
       if (temp_buffer[i] > 1)
          temp_buffer[i] = 1;
       else   
           if (temp_buffer[i] < -1)
              temp_buffer[i] = -1;
      }    

*/


//something wrong with the meter
//need to correct peaks falloff

//get max values


  if (documents->current->wave_edit->waveform->sound_buffer->channels == 1)
     for (size_t i = 0; i < nsamples; i++)
         {
          if (float_greater_than (temp_buffer[i], maxl))
              maxl = temp_buffer[i];
         }
           
       
//panning factors, left and right:
        float panl = 0.0;
        float panr = 0.0;
      
      //use the selected panner to calculate
        //panl and panr
            
        if (panner == 0)
            pan_linear0 (panl, panr, pan);
        else
            if (panner == 1)
                pan_linear6 (panl, panr, pan);
        else
             if (panner == 2)
              pan_sqrt (panl, panr, pan);
	    else
            if (panner == 3)
               pan_sincos (panl, panr, pan);
       
           
  size_t i = 0;
  
  if (documents->current->wave_edit->waveform->sound_buffer->channels == 2)
     do
       {
       
        if (float_less_than (maxl, temp_buffer[i]))//(maxl < temp_buffer[i])
            maxl = temp_buffer[i];
            
        if (! bypass_mixer)    
            temp_buffer[i] = temp_buffer[i] * panl * gain;  
            
        if (! play_l)
           temp_buffer[i] = 0;
            
        i++;
        if (float_less_than (maxr, temp_buffer[i]))//(maxr < temp_buffer[i])
            maxr = temp_buffer[i];
        
        if (! bypass_mixer)    
           temp_buffer[i] = temp_buffer[i] * panr * gain;  
        
        
       if (! play_r)
           temp_buffer[i] = 0;
                
        i++;
       }
    while (i < nsamples);



  if (wnd_fxrack->level_meter)         
     {
      wnd_fxrack->level_meter->pl = maxl;
      wnd_fxrack->level_meter->pr = maxr;
     }
                

  documents->current->wave_edit->waveform->sound_buffer->buffer_offset += nsamples;

  return nframes;
}


size_t CDSP::process_rec (float *buffer, size_t channels, size_t nframes)
{
  //qDebug() << "CDSP::process -- start";
  
  if (nframes == 0)
     return 0;

  if (transport_state == STATE_EXIT)
     return 0;

   
  maxl = 0.0f;
  maxr = 0.0f;

//here we work with short buffer to process it and output to it

  size_t nsamples = nframes * channels;
                
  if (channels == 1)
     for (size_t i = 0; i < nsamples; i++)
         {
          if (float_greater_than (buffer[i], maxl))
              maxl = buffer[i];
         }
           
           
  size_t i = 0;

  if (channels == 2)
     do
       {
        if (float_less_than (maxl, buffer[i]))
            maxl = buffer[i];
        i++;
        if (float_less_than (maxr, buffer[i]))
            maxr = buffer[i];
        i++;
       }
    while (i < nsamples);


  if (wnd_fxrack->level_meter)         
     {
      wnd_fxrack->level_meter->pl = maxl;
      wnd_fxrack->level_meter->pr = maxr;
     }
                

  return nframes;
}


CEnvelope::~CEnvelope()
{
  for (int i = 0; i < points.size(); i++)
     delete points[i];
}



bool comp_ep (CEnvelopePoint *e1, CEnvelopePoint *e2)
{
    return e1->position < e2->position;
}



void CEnvelope::insert_wise (int x, int y, int height, size_t maximum)
{

  CEnvelopePoint *e = new CEnvelopePoint;
  
  e->position = x;
  e->value = get_percent (height, (float)y);

 qDebug() << "max: " << maximum;
  
  if (points.size() == 0)
     {
      qDebug() << "create envelope";
      
      CEnvelopePoint *point_start = new CEnvelopePoint;
  
      point_start->position = 0;
      point_start->value = 50.0f;
    
      CEnvelopePoint *point_end = new CEnvelopePoint;
  
      point_end->position = maximum - 1;
      point_end->value = 50.0f;
    
      points.append (point_start);
     
      points.append (e);
      
      points.append (point_end);

      qDebug() << "point_start.pos: " << point_start->position;
      qDebug() << "point_start.val: " << point_start->value;


      qDebug() << "e.pos: " << e->position;
      qDebug() << "e.val: " << e->value;

      qDebug() << "point_end.pos: " << point_end->position;
      qDebug() << "point_end.val: " << point_end->value;

      
      qDebug() << "points.size() == " <<  points.size();
 

      return;
     }

  //is there duplicated position? if yes, replace it with a new item
  for (int i = 0; i < points.size(); i++)
      {
       if (e->position == points[i]->position)
          {
           qDebug() << "dup point at " << i; 
           delete points[i];
           points.removeAt (i);
           return;
          }
      } 
       
  points << e;
 // qSort (points.begin(), points.end());
 
  qStableSort (points.begin(), points.end(), comp_ep);

}


void CEnvelope::point_move (CEnvelopePoint *p, int x, int y, int height)
{
  if (! p)
     return;

  p->position = x;
  p->value = get_percent (height, (float)y);

  qDebug() << "move point to x: " << p->position << " val: " << p->value;
 /*
  for (int i = 0; i < points.size(); i++)
      {
       if (p->position == points[i]->position && p->value != points[i]->value)
          {
           qDebug() << "dup point at " << i; 
           delete points[i];
           points.removeAt (i);
           return;
          }
      } 
       
 
  qStableSort (points.begin(), points.end(), comp_ep);
*/

  qStableSort (points.begin(), points.end(), comp_ep);

}


CEnvelopePoint* CEnvelope::get_selected()
{
  CEnvelopePoint *p = 0;
  
  
  for (int i = 0; i < points.size(); i++)
      {
       if (points[i]->selected)
          {
           p = points[i];
           break;
          }

      } 
       
  return p;

}


void CWaveform::prepare_image()
{
  //qDebug() << "CWaveform::prepare_image() - start";

  if (! sound_buffer->buffer)
     {
      qDebug() << "! buffer";
      return;
     } 

  size_t sections = section_to - section_from;
  int image_height = height();
  int channel_height = image_height / sound_buffer->channels;
  
  if (minmaxes)
     delete minmaxes;

  minmaxes = new CMinmaxes (sound_buffer->channels, sections);
   
  size_t frame = 0;

  size_t section = 0;

  size_t sampleno = section_from * (frames_per_section * sound_buffer->channels) - 1;
 
  while (section + 1 < sections)
        {
         for (int ch = 0; ch < sound_buffer->channels; ch++)    
             {
              sampleno++;

              if (sound_buffer->buffer[sampleno] < minmaxes->values[ch]->min_values->temp)
                 minmaxes->values[ch]->min_values->temp = sound_buffer->buffer[sampleno];
              else   
              if (sound_buffer->buffer[sampleno] > minmaxes->values[ch]->max_values->temp)
                 minmaxes->values[ch]->max_values->temp = sound_buffer->buffer[sampleno];
             }
           

         if (frame == frames_per_section)
            {
             section++;
             frame = 0;
             for (int ch = 0; ch < sound_buffer->channels; ch++)    
                 { 
                  minmaxes->values[ch]->min_values->samples[section] = minmaxes->values[ch]->min_values->temp;
                  minmaxes->values[ch]->max_values->samples[section] = minmaxes->values[ch]->max_values->temp;
               
                  minmaxes->values[ch]->min_values->temp = 0;
                  minmaxes->values[ch]->max_values->temp = 0;
                 }

             continue;
            }

          frame++;
         }

  QImage img (width(), image_height, QImage::Format_RGB32);
  QPainter painter (&img);
  
  painter.setPen (cl_waveform_foreground);
    
  img.fill (cl_waveform_background.rgb()); 
    
  for (int ch = 0; ch < sound_buffer->channels; ch++)    
  for (size_t x = 0; x < sections; x++)
      {
       int ymax = channel_height - (int)((minmaxes->values[ch]->max_values->samples[x] + 1) * 0.5f * channel_height);
       int ymin = channel_height - (int)((minmaxes->values[ch]->min_values->samples[x] + 1) * 0.5f * channel_height);

  	   QPoint p1 (x, ymin + channel_height * ch);
       QPoint p2 (x, ymax + channel_height * ch);
  		   /*
  		   if ((minmaxes->values[ch]->max_values->samples[x] >= 1) || (minmaxes->values[ch]->min_values->samples[x] <= -1))
       painter.setPen (QColor (Qt::red));
       else
       painter.setPen (cl_waveform_foreground);
*/

       painter.drawLine (p1, p2);
      }        


//draw the amplitude meter bar  
//FIXME!!!
  painter.setPen (cl_text);
  //painter.setFont (QFont ("Mono", 5));

  QFont tfont ("Mono");
  tfont.setPixelSize (8);

  painter.setFont (tfont);

   
  //int ten = get_value (channel_height / 2, 10) / 2;  
       
  for (int ch = 0; ch < sound_buffer->channels; ch++)    
      {
      
      //draw axis
       int yaxe = image_height / (ch + 1) - channel_height / 2;
       
       QPoint p1 (0, yaxe);
       QPoint p2 (width(), yaxe);
       painter.drawLine (p1, p2);
                 
       
       int y_channel_origin = channel_height * (ch + 1) - channel_height;
    
       int db = -27;
    
       while (db <= 0)
           {
            float linval = db2lin (db);
    
            
            int ymin = channel_height - (int)((linval + 1) * .5 * channel_height) + y_channel_origin;
            int y = ymin;  

            QPoint p1 (1, y);
            QPoint p2 (5, y);
            painter.drawLine (p1, p2);
       
            if (draw_shadow)
               {    
                painter.setPen (cl_shadow);
                painter.drawText (QPoint (2, 1 + y), QString::number (db, 'g', 2)); //dB
                painter.setPen (cl_text);
               }                  
                    
            painter.drawText (QPoint (1, y), QString::number (db, 'g', 2)); //dB
         
            if (db >= -9)
               db++;
            else
                db += 5;
    
           }            
       }  
  
  //draw vol env
  
  int old_point_x;
  int old_point_y;
  
  for (int i = 0; i < env_vol.points.size(); i++)
      {
       size_t sample_start = section_from * frames_per_section * sound_buffer->channels;
       size_t sample_end = section_to * frames_per_section * sound_buffer->channels;
       
       if (env_vol.points[i]->position < sample_end && env_vol.points[i]->position > sample_start)
          {
           //draw point at:
           
           //qDebug() << "draw point at:" << env_vol[i]->position;

            
           int pos_sections = env_vol.points[i]->position / sound_buffer->channels / frames_per_section; 
           
           int point_x = pos_sections - section_from; //in sections
          
    
           //int point_y = env_vol.points[i]->value;
           
           int point_y = get_fvalue (image_height, env_vol.points[i]->value);
           
           
           
  //         qDebug() << "point_x " << point_x; 
 
          painter.setPen (cl_envelope);

                     
           if (i < env_vol.points.size())          
              {
               painter.drawLine (old_point_x, old_point_y, point_x, point_y);
              
              }
 
           QRect rectangle (point_x, point_y, ENVELOPE_SIDE, ENVELOPE_SIDE); 
 
           if (env_vol.points[i]->selected)
              {
                painter.setPen (QColor (cl_env_point_selected));
              }
           else   
                painter.setPen (cl_envelope);
              
  
           painter.drawRect (rectangle);
           painter.drawText (QPoint (point_x + 1, point_y + 1), QString::number (point_y));
          
           old_point_x = point_x;
           old_point_y = point_y;
          }
      }
  
  
  waveform_image = img;

  //qDebug() << "CWaveform::prepare_image() - end";
}


void CEnvelope::select_point (CEnvelopePoint *e)
{
  foreach (CEnvelopePoint *t, points)
          {
           t->selected = false;
          }
          
   e->selected = true;       
}


CEnvelopePoint* CEnvelope::find (int frame, int y, int height, int frames_per_section)
{
  foreach (CEnvelopePoint *t, points)
          {
           int t_y = get_fvalue (height, t->value);
           int t_x = t->position;
           
           qDebug() << "t_x: " << t_x << " t_y: " << t_y; 

           QRect rectangle (t_x, t_y, frames_per_section * ENVELOPE_SIDE, ENVELOPE_SIDE); 
           
           if (rectangle.contains (frame, y))
              {
               qDebug() << "ok";
               return t; 
              } 
          }
          
  return 0;        
}



int CWaveform::get_cursor_position()
{
  if (frames_per_section == 0 || sound_buffer->channels == 0)
     return 0;

  return sound_buffer->buffer_offset / sound_buffer->channels / frames_per_section;
}


int CWaveform::get_selection_start()
{
 return sel_start_samples / sound_buffer->channels / frames_per_section;
}


int CWaveform::get_selection_end()
{
 return sel_end_samples / sound_buffer->channels / frames_per_section;
}


void CWaveform::set_cursor_by_section (size_t section)
{
  sound_buffer->buffer_offset = section * frames_per_section * sound_buffer->channels;
}



void CWaveform::mouseDoubleClickEvent (QMouseEvent *event)
{
  select_all();
  event->accept();
}


void CWaveform::mousePressEvent (QMouseEvent *event)  
{
  if (! sound_buffer->buffer)
     {
      event->accept();
      return;
     }
  

  setFocus (Qt::OtherFocusReason);
  mouse_pressed = true;

  int section = section_from + event->x();

  if (event->button() == Qt::RightButton)
     {
      env_vol.insert_wise (section * frames_per_section * sound_buffer->channels, event->y(), height(), sound_buffer->buffer_size);
    
      setFocus (Qt::OtherFocusReason);
 
      recalc_view();
      prepare_image();
      
      update();

      previous_mouse_pos_x = section;
      return;
     }

  
  CEnvelopePoint *ep = env_vol.find (section * frames_per_section * sound_buffer->channels, event->y(), height(), frames_per_section);

  if (ep)
     {
      //qDebug() << "ep->position = " << ep->position;
      //qDebug() << "ep->value = " << ep->value;
      env_vol.select_point (ep);
  
      recalc_view();
      prepare_image();
      
      update();

      previous_mouse_pos_x = section;
      return;
     }
  


//deselect if click is not on the selection bounds
  if (! x_nearby (section, get_selection_start(), 2) &&
      ! x_nearby (section, get_selection_end(), 2))
      {
       deselect();
       set_cursor_value (section);
      } 
  else
      {
       //selected = true;

       if (x_nearby (section, get_selection_start(), 2))
          {
           set_selstart_value (section);
           selection_selected = 1;
  //         qDebug() << "selection_selected: " << selection_selected; 
          } 
          
       else  
       if (x_nearby (section, get_selection_end(), 2))
          {
           set_selend_value (section);
           selection_selected = 2;
//           qDebug() << "selection_selected: " << selection_selected; 

          }
           
      }   
  
  
  if (! selected)
      set_cursor_value (section);
    
  update();
  
  set_cursorpos_text();
  previous_mouse_pos_x = section;

}


void CWaveform::mouseMoveEvent (QMouseEvent *event)
{
  if (! sound_buffer->buffer)
     {
      event->accept();
      return;
     }

  if (mouse_pressed)
    {
     if (event->pos().x() > width())
        {
         if (scrollbar->value() + 16 != scrollbar->maximum())
            scrollbar->setValue (scrollbar->value() + 16);
     }

     if (event->pos().x() < 0)
        {
         if (scrollbar->value() - 16 != 0)
             scrollbar->setValue (scrollbar->value() - 16);
        }
    }
  
  int x = event->x();
  if (x < 0)
     x = 0;

  if (x > width() - 1)
     x = width() - 1;

  size_t current_section = section_from + x;

    if (! mouse_pressed)
     {
      if (x_nearby (current_section, get_selection_start(), 1) ||
         x_nearby (current_section, get_selection_end(), 1))
        {
         setCursor (Qt::SizeHorCursor);
         normal_cursor_shape = false;
        }
     else
         if (! normal_cursor_shape)
           {
            normal_cursor_shape = true;
            setCursor(Qt::ArrowCursor);
           }
     }

 
 //захват огибающей
 
 if (mouse_pressed)
   {
    //CEnvelopePoint *ep = env_vol.find (current_section * frames_per_section * sound_buffer->channels, event->y(), height(), frames_per_section);

     CEnvelopePoint *ep = env_vol.get_selected();

     if (ep)
     //if (ep->selected)
     {
      //qDebug() << "ep->position = " << ep->position;
      //qDebug() << "ep->value = " << ep->value;
      env_vol.point_move (ep, current_section * frames_per_section * sound_buffer->channels, event->y(), height());
  
      recalc_view();
      prepare_image();
      
      update();

     // previous_mouse_pos_x = section;
      return;
     }
  }
 
   
  //условия возникновения создания выделения, если нет такового
  if (! selected)
  if (mouse_pressed)
     {
     // qDebug() << "create selection";
      if (current_section < previous_mouse_pos_x)
         {
          set_selstart_value (current_section);
          set_selend_value (previous_mouse_pos_x);
          selection_selected = 1;
         }
      else   
      if (current_section > previous_mouse_pos_x)
         {
          set_selstart_value (previous_mouse_pos_x);
          set_selend_value (current_section);
          selection_selected = 2;
         }
      
      selected = true;
      return;
     } 
   
   
  if (mouse_pressed)
     {
      if (selection_selected == 1)
          set_selstart_value (current_section);
      else
          if (selection_selected == 2)
          set_selend_value (current_section);

      fix_selection_bounds();

      set_statusbar_text();

      update();
     }

  previous_mouse_pos_x = current_section;

  QWidget::mouseMoveEvent (event);
}


void CWaveform::mouseReleaseEvent (QMouseEvent * event)
{
  if (! sound_buffer->buffer)
     {
      event->accept();
      return;
     }

  mouse_pressed = false;

  selection_selected = 0;
  
//  if (selected)
  //   fix_selection_bounds_release();

  QWidget::mouseReleaseEvent (event);
}
