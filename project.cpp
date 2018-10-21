#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include <QFileDialog>
#include <QXmlStreamWriter>
#include <QGraphicsProxyWidget>
#include <QTimeEdit>
#include <QScrollArea>
#include <QDateTime>
#include <QSlider>
#include <QErrorMessage>
#include <QCheckBox>
#include <QInputDialog>
#include <QDial>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QImage>
#include <QPainter>


#include <QMessageBox>

#include <typeinfo>
#include <limits>

#include <sndfile.h>
#include <samplerate.h>
#include <portaudio.h>
#include <string.h>


#include "db.h"
#include "fxlist.h"

#include "tio.h"
#include "project.h"
#include "utils.h"
#include "fx-panners.h"

#define TIME_FMT "hh:mm:ss.zzz"

#define RENDER_MODE_REALTIME 0
#define RENDER_MODE_OFFLINE 1


extern CFxList *avail_fx;


CTioHandler *tio_handler;
CWAVPlayer *wav_player;

int pa_device_id_in;
int pa_device_id_out;

bool b_monitor_input;
int mono_recording_mode;
int rec_channels;


PaStream *pa_stream_out;

PaStream *mixbuf_stream;


size_t global_samplerate;
size_t buffer_size_frames;
size_t buffer_size_frames_multiplier;


#define TRACKCONTROLW 0


//CFloatBuffer *fb_stereo_rec;

bool comp_clips (CClip *c1, CClip *c2)
{
  return c1->position_frames < c2->position_frames;
}


size_t timestring_to_frames (const QString &val)
{
  QTime t = QTime::fromString (val, TIME_FMT);
  
  int hours_mseconds = t.hour() * 3600000;
  int minutes_mseconds = t.minute() * 60000;
  int msecs_mseconds = t.second() * 1000;
  int mseconds = t.msec();
  
  int mseconds_total = hours_mseconds + minutes_mseconds + msecs_mseconds + mseconds;
 
  return msecs_to_frames (mseconds_total, global_samplerate);
}


void CProject::project_new (const QString &fname)
{
  qDebug() << "CProject::project_new";

  QFileInfo f (fname);

  paths.project_dir = f.dir().absolutePath() + "/" + f.baseName(); 

 // qDebug() << "paths.project_dir: " << paths.project_dir;
  
  paths.wav_dir = paths.project_dir + "/wav";
  paths.xml_dir = paths.project_dir + "/xml";
  paths.output_dir = paths.project_dir + "/output";
  
  QDir dr;
  dr.setPath (paths.project_dir);
  if (! dr.exists())
     dr.mkpath (paths.project_dir);
    
  
  dr.setPath (paths.wav_dir);
  if (! dr.exists())
     dr.mkpath (paths.wav_dir);

  dr.setPath (paths.xml_dir);
  if (! dr.exists())
     dr.mkpath (paths.xml_dir);
  
  dr.setPath (paths.output_dir);
  if (! dr.exists())
     dr.mkpath (paths.output_dir);
  
  paths.fname_project = paths.project_dir + "/" + f.baseName() + ".wvn";


      // qstring_save (paths.fname_project, "test=1", "UTF-8");
      
  settings.call_ui (true);

  //settings.save (paths.fname_project);

  tab_project_ui->addTab (w_timeline, tr ("timeline"));
  tab_project_ui->addTab (mixer_window, tr ("mixer"));
  tab_project_ui->addTab (table_container, tr ("table"));
}


  
CProjectManager::CProjectManager()
{
  project = 0;
  
  tio_handler = new CTioHandler;

  wav_player = new CWAVPlayer;

 // fb_stereo_rec = 0;

}


CProjectManager::~CProjectManager()
{
  if (project)
     delete project;
     
  delete tio_handler;   
  delete wav_player; 
  
 // delete fb_stereo_rec;
}


void CProjectManager::create_new_project (const QString &fname)
{
  if (project)
     delete project;
     
  project = new CProject;   
  project->tab_project_ui = tab_project_ui;
  project->project_new (fname);
}



bool CProject::project_open (const QString &fname)
{
  qDebug() << "CProject::project_open: " << fname;

  if (! file_exists (fname))
     {
      qDebug() << "! file_exists " << fname;
      return false;
     }  

  if (! fname.endsWith (".wvn"))
     return false;

  QFileInfo f (fname);

  paths.project_dir = f.dir().absolutePath(); 

//  qDebug() << "load paths.project_dir: " << paths.project_dir;
  
  paths.wav_dir = paths.project_dir + "/wav";
  paths.xml_dir = paths.project_dir + "/xml";
  paths.output_dir = paths.project_dir + "/output";
  
  QDir dr;
  dr.setPath (paths.project_dir);
  if (! dr.exists())
     dr.mkpath (paths.project_dir);
    
  
  dr.setPath (paths.wav_dir);
  if (! dr.exists())
     dr.mkpath (paths.wav_dir);

  dr.setPath (paths.xml_dir);
  if (! dr.exists())
     dr.mkpath (paths.xml_dir);
  
  dr.setPath (paths.output_dir);
  if (! dr.exists())
     dr.mkpath (paths.output_dir);
  
  paths.fname_project = paths.project_dir + "/" + f.baseName() + ".wvn";

  tab_project_ui->addTab (w_timeline, tr ("timeline"));
  tab_project_ui->addTab (mixer_window, tr ("mixer"));
  tab_project_ui->addTab (table_container, tr ("table"));

     
  files.load_all_from_dir (paths.wav_dir); 

  if (file_exists (paths.fname_project))
    load_project();  
    
  return true;
}



bool CProject::wav_copy (const QString &fname)
{
//  if (! project)
  //   return false;

   CTio *tio = tio_handler->get_for_fname (fname);

  if (! tio)
     {
      //holder->log->log (tr ("file type of %1 is not supported")
        //                   .arg (fileName));
        
      qDebug() << "NOT A SOUND";  
      return false;
     }
  
  qDebug() << "SOUNDS OK";
 
  QFileInfo fi (fname);
  QString fname_dest;
      
      
  fname_dest += paths.wav_dir + "/" + fi.baseName() + ".wav";
  
  qDebug() << "fname_dest:" << fname_dest;     
      
      
 CFloatBuffer *fb = tio->load (fname);
  
  if (! fb)
     {
      //holder->log->log (tr ("cannot open %1 because of: %2")
        //                   .arg (fileName)
          //                 .arg (tio->error_string));
      return false;
     }

  //int frames = tio->frames;    
  

  bool need_to_resample = false;

 if (fb->samplerate != settings.samplerate)
    need_to_resample = true;

  SF_INFO sf;

  sf.samplerate = settings.samplerate;
  sf.channels = fb->channels;
  sf.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
  
  if (! sf_format_check (&sf))
     {
      qDebug() << "! sf_format_check (&sf)";
      return false;  
     }  
  
  
  //resample
  
   if (need_to_resample)
   {
    CFloatBuffer *tbf = fb->resample (settings.samplerate);
    delete fb;
    fb = tbf;

  }

//////
  
  fb->allocate_interleaved();
  fb->fill_interleaved();
  
  SNDFILE *file = sf_open (fname_dest.toUtf8().data(), SFM_WRITE, &sf);
  
  sf_count_t zzz = sf_writef_float (file, fb->buffer_interleaved, fb->length_frames);
  
  qDebug() << "zzz:" << zzz;
  
  
  sf_close (file);
      
  delete fb;
  
      
  return true;
}



bool CProjectManager::project_open (const QString &fname)
{
  qDebug() << "CProjectManager::project_open: " << fname;

  if (file_get_ext (fname) != "wvn")
     {
      //check is sound readable
      qDebug() << "fname:" << fname;
      
      if (project)
         return project->wav_copy (fname); 
      else
          return false;   
     }

  if (project)
     delete project;
     
  project = new CProject;   
  project->tab_project_ui = tab_project_ui;
  project->project_open (fname);
  
  return true;
}



CProjectSettings::CProjectSettings()
{
 // qDebug() << "CProjectSettings::CProjectSettings()";

  panner = 0;
  settings_window = 0;
  samplerate = 44100;
  global_samplerate = samplerate;
  bpm = 120;
}


void CProjectSettings::ui_done()
{
  samplerate = ed_samplerate->text().toInt(); 
  bpm = ed_bpm->text().toInt(); 
  panner = cmb_panner->currentIndex();
  
  global_samplerate = samplerate;
       
  settings_window->close();
//  settings_window = 0;
}



void CProjectSettings::call_ui (bool creation)
{
   settings_window = new QDialog;
  
//   settings_window->setModal (true);  
  
   settings_window->setWindowTitle (tr ("Project settings"));
  
   QVBoxLayout *v_box = new QVBoxLayout;
   settings_window->setLayout (v_box);
  
   QHBoxLayout *h_samplerate = new QHBoxLayout;

   QLabel *l_samplerate = new QLabel (tr ("Samplerate"));
   ed_samplerate = new QLineEdit;

   ed_samplerate->setText (QString::number (samplerate));

   if (! creation)
      ed_samplerate->setEnabled (false);

   h_samplerate->addWidget (l_samplerate);
   h_samplerate->addWidget (ed_samplerate);

   QHBoxLayout *h_bpm = new QHBoxLayout;

   QLabel *l_bpm = new QLabel (tr ("BPM"));
   ed_bpm = new QLineEdit;

   ed_bpm->setText (QString::number (bpm));

   if (! creation)
      ed_bpm->setEnabled (false);

   h_bpm->addWidget (l_bpm);
   h_bpm->addWidget (ed_bpm);


  QHBoxLayout *h_panner = new QHBoxLayout;
  QLabel *l_panner = new QLabel (tr ("Panner:"));
    
  cmb_panner = new QComboBox;
  
  QStringList panners;
  panners.append (tr ("linear, law: -6 dB"));
  panners.append (tr ("linear, law: 0 dB"));
  panners.append (tr ("square root, law: -3 dB"));
  panners.append (tr ("sin/cos, law: -3 dB"));
  
  cmb_panner->addItems (panners);
  
  h_panner->addWidget (l_panner);
  h_panner->addWidget (cmb_panner);
  
  cmb_panner->setCurrentIndex (panner);
  
  v_box->addLayout (h_samplerate);
  v_box->addLayout (h_bpm);
  v_box->addLayout (h_panner);
 
  QPushButton *bt_done = new QPushButton (tr ("Done"), settings_window);
  connect (bt_done, SIGNAL(clicked()), this, SLOT(ui_done()));

   v_box->addWidget (bt_done);
   
   settings_window->exec();
}


CWavFiles::~CWavFiles()
{
  QList <CWAVSource *> values = wavs.values();

 for (int i = 0; i < values.size(); i++)
      delete values[i];
}


void CWavFiles::load_file (const QString &path, const QString &f)
{
  CWAVSource *ws = new CWAVSource;
  ws->load (path + "/" + f); 
  wavs [f] = ws;
}
          

void CWavFiles::load_all_from_dir (const QString &path)
{
  wavs.clear();
  
  QStringList l = read_dir_files (path);
  
  foreach (QString f, l) 
          {
           CWAVSource *ws = new CWAVSource;
           ws->load (path + "/" + f); 
           wavs [f] = ws;
          }
}


void CWAVSource::load (const QString &path)
{
  qDebug() << "load: " << path;

  CTio *tio = tio_handler->get_for_fname (path);

  if (! tio)
     {
      //holder->log->log (tr ("file type of %1 is not supported")
        //                   .arg (fileName));
        
      qDebug() << "NOT A SOUND";  
      return;
     }
 
  fb = tio->load (path);
  
  if (! fb)
     {
      //holder->log->log (tr ("cannot open %1 because of: %2")
        //                   .arg (fileName)
          //                 .arg (tio->error_string));
      return;
     }

}


void CProject::lw_wavs_refresh()
{
  lw_wavs->clear();
  lw_wavs->addItems (read_dir_files (paths.wav_dir));
}


void CProject::bt_wavs_add_click()
{
  QStringList fileNames;

  QFileDialog dialog;
  dialog.setOption (QFileDialog::DontUseNativeDialog, true);
  
  if (dialog.exec())
     {
      fileNames = dialog.selectedFiles();
      
      foreach (QString fn, fileNames)
              {
               wav_copy (fn);
               
               QFileInfo fi (fn);
               
               files.load_file (paths.wav_dir, fi.fileName());        
              }
              
      lw_wavs_refresh();        
     }
}


void CProject::bt_wavs_edit_click()
{


}


void CProject::bt_wavs_clip_click()
{
//qDebug() << "CProject::bt_wavs_clip_click()";

  QListWidgetItem *item = lw_wavs->currentItem();
  if (! item)
      return;
     
  CWAVSource *source = files.wavs[item->text()];   
  
  if (! source)
      return;

  QWidget *w = table_container->focusWidget();

  if (! w)
      return;

  if (typeid(*w) == typeid(CTrackTableWidget)) 
    {
     CTrackTableWidget *p = (CTrackTableWidget*)w;
     
     if (p->p_track->channels != files.wavs[item->text()]->fb->channels)
        {
         QErrorMessage m;
         m.showMessage (tr ("Track cnannels <> wav channels count"));
        
         qDebug() << "p->p_track->channels != files.wavs[item->text()]->channels";
         return;
        }
     
     CWaveClip *clip = new CWaveClip (p->p_track);
     clip->filename = item->text();
     clip->file = files.wavs[item->text()];   
     
     clip->muted = false;
        
     clip->offset_frames = 0;
     clip->length_frames = clip->file->fb->length_frames;

     //а какую позицию выбрать?
     clip->position_frames = cursor_frames;     
     
     QDateTime dt = QDateTime::currentDateTime();
     
     clip->name = clip->filename + "-" + dt.toString ("yyyy-MM-dd@hh:mm:ss:zzz");
     
     p->p_track->clips.append (clip);
         
     
     p->update_track_table();
     refresh_song_length();
    }
   
}


void CProject::bt_wavs_play_click()
{
//  QPushButton* button = dynamic_cast<QPushButton*>(sender());

  if (! wav_player)
     return;
  
  QListWidgetItem *item = lw_wavs->currentItem();
  if (! item)
      return;
     
  CWAVSource *source = files.wavs[item->text()];   
  
  if (! source)
      return;
     
  wav_player->link (source->fb);
  wav_player->play();        
}


void CProject::bt_wavs_del_click()
{
//qDebug() << "CProject::bt_wavs_del_click()";

  QListWidgetItem *item = lw_wavs->currentItem();
  if (! item)
     return;

  QString fname = item->text(); 
  QString path = paths.wav_dir + "/" + fname;

  QFile f (path);
  f.remove();
  lw_wavs_refresh();

//похерить объект в files!!!


  CWAVSource *ws = files.wavs.take (fname);
  if (ws)
     delete (ws);


//remove all linked clips:

  for (int i = 0; i < tracks.size(); i++)
      {
       CTrack *p_track = tracks[i];
       
       QMutableListIterator <CClip*> it (p_track->clips);
       
       while (it.hasNext()) 
             {
              CWaveClip *cw = (CWaveClip*)it.next();
              if (cw->filename == fname)
                 {
                  it.remove();
                  delete cw;
                 } 
             }
      
       CTrackTableWidget *w = (CTrackTableWidget*)p_track->table_widget;
       w->update_track_table();
      }
  
}


void CProject::bt_wavclip_add_click()
{


}


void CProject::bt_wavclip_play_click()
{
  
}


void CProject::bt_wavclip_edit_click()
{


}


void CProject::bt_wavclip_del_click()
{


}



void CProject::files_window_show()
{
  lw_wavs_refresh();

  if (! wnd_files->isVisible())
     wnd_files->show();
  else
     wnd_files->close();
}


void CProject::files_window_create()
{

  wnd_files = new QWidget;
  wnd_files->setWindowTitle (tr ("WAVs"));

  QHBoxLayout *h_main = new QHBoxLayout;
  wnd_files->setLayout (h_main);
  
  QGroupBox *gb_wavs = new QGroupBox (tr ("WAVs"));
  QVBoxLayout *v_wavs = new QVBoxLayout;
  gb_wavs->setLayout (v_wavs);
 
  lw_wavs = new QListWidget;
  v_wavs->addWidget (lw_wavs);
  
  QHBoxLayout *h_wavs_buttons = new QHBoxLayout;
  v_wavs->addLayout (h_wavs_buttons);
  
  QPushButton *bt_wavs_add = new QPushButton ("Import");
  QPushButton *bt_wavs_play = new QPushButton("Play");
  
  QPushButton *bt_wavs_edit = new QPushButton ("Edit");
  QPushButton *bt_wavs_clip = new QPushButton ("To clip");
  
  QPushButton *bt_wavs_del = new QPushButton ("Delete");
    
  connect (bt_wavs_add, SIGNAL(clicked()), this, SLOT(bt_wavs_add_click()));  
  connect (bt_wavs_play, SIGNAL(clicked()), this, SLOT(bt_wavs_play_click()));  
  connect (bt_wavs_clip, SIGNAL(clicked()), this, SLOT(bt_wavs_clip_click()));  
  
  connect (bt_wavs_edit, SIGNAL(clicked()), this, SLOT(bt_wavs_edit_click()));  
  connect (bt_wavs_del, SIGNAL(clicked()), this, SLOT(bt_wavs_del_click()));  
    
    
  h_wavs_buttons->addWidget (bt_wavs_add);  
  h_wavs_buttons->addWidget (bt_wavs_play);  
  h_wavs_buttons->addWidget (bt_wavs_clip);  
  
  h_wavs_buttons->addWidget (bt_wavs_edit);  
  h_wavs_buttons->addWidget (bt_wavs_del);  
    
  h_main->addWidget (gb_wavs);
  
  
  /*
  QGroupBox *gb_wavclips = new QGroupBox (tr ("Clips"));
  QVBoxLayout *v_wavclips = new QVBoxLayout;
  gb_wavclips->setLayout (v_wavclips);
 
  lw_wavclips = new QListWidget;
  v_wavclips->addWidget (lw_wavclips);
  
  QHBoxLayout *h_wavclips_buttons = new QHBoxLayout;
  v_wavclips->addLayout (h_wavclips_buttons);
  
  QPushButton *bt_clip_add = new QPushButton ("+");
  QPushButton *bt_clip_play = new QPushButton (">");
  
  QPushButton *bt_clip_edit = new QPushButton ("*");
  QPushButton *bt_clip_del = new QPushButton ("-");
    
  h_wavclips_buttons->addWidget (bt_clip_add);  
  h_wavclips_buttons->addWidget (bt_clip_play);  
  h_wavclips_buttons->addWidget (bt_clip_edit);  
  h_wavclips_buttons->addWidget (bt_clip_del);  
  
  h_main->addWidget (gb_wavclips);
  */
 

/*
  QVBoxLayout *lt_vkeys = new QVBoxLayout;
  QVBoxLayout *lt_vbuttons = new QVBoxLayout;

  lv_menuitems = new QListWidget;
  opt_update_keyb();
  
  lt_vkeys->addWidget (lv_menuitems);

  connect (lv_menuitems, SIGNAL(currentItemChanged (QListWidgetItem *, QListWidgetItem *)),
           this, SLOT(slot_lv_menuitems_currentItemChanged (QListWidgetItem *, QListWidgetItem *)));
*/
}


CWAVPlayer::CWAVPlayer()
{
  p_buffer = 0;
  offset_frames = 0;
  state = 0;
}


void CWAVPlayer::link (CFloatBuffer *a_p_buffer)
{
  offset_frames = 0;
  state = 0;

  p_buffer = a_p_buffer;
}


int wavplayer_pa_stream_callback (const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
  
  if (! wav_player)   
     return paAbort;
     
  if (wav_player->offset_frames >= wav_player->p_buffer->length_frames)   
     return paAbort;
    
//  qDebug() << "wav_player->offset_frames: " << wav_player->offset_frames;   
//  qDebug() << "wav_player->p_buffer->length_frames: " << wav_player->p_buffer->length_frames;   
 
  float **outb = (float **)output;
 
  size_t tail = wav_player->p_buffer->length_frames - wav_player->offset_frames;
  size_t sz = frameCount;
  
  if (sz > tail)
      sz = tail;
 
  for (size_t ch = 0; ch < wav_player->p_buffer->channels; ch++)
      {
       memcpy (outb[ch], wav_player->p_buffer->buffer[ch] + wav_player->offset_frames,
       sz * sizeof (float));
      }
  
  wav_player->offset_frames += frameCount;

  return paContinue; 	
}


void CWAVPlayer::play()
{
  if (pa_stream_out)
     {
      Pa_CloseStream (pa_stream_out);	
      pa_stream_out = 0;
     }
  
  if (! p_buffer)
     {
      qDebug() << "! p_buffer"; 
      return;
     } 
 
  wav_player->offset_frames = 0;
  
  PaStreamParameters outputParameters;

  outputParameters.device = pa_device_id_out;
  outputParameters.channelCount = p_buffer->channels;
  
  outputParameters.sampleFormat = paFloat32;
  outputParameters.sampleFormat |= paNonInterleaved;
  
  outputParameters.suggestedLatency = Pa_GetDeviceInfo (outputParameters.device)->defaultLowOutputLatency;;
  outputParameters.hostApiSpecificStreamInfo = NULL;
  
  PaError err = Pa_OpenStream (&pa_stream_out,
                               NULL,
                               &outputParameters,
                               p_buffer->samplerate,
	                            buffer_size_frames,
		                       paNoFlag,
		                       wavplayer_pa_stream_callback,
		                       NULL//&pe 
	                          ); 	
   
  qDebug() << Pa_GetErrorText (err);
       
  if (err < 0)
     {
      qDebug() << (Pa_GetErrorText (err));
      pa_stream_out = 0;
      return;
     }
     
      
  err = Pa_StartStream (pa_stream_out);
  qDebug() << Pa_GetErrorText (err);

  if (err < 0)
     {
      qDebug() << (Pa_GetErrorText (err));
      pa_stream_out = 0;
     }
}


CWAVPlayer::~CWAVPlayer()
{
 if (pa_stream_out)
     {
      Pa_CloseStream (pa_stream_out);	
      pa_stream_out = 0;
     }
}


CProject::~CProject()
{
  if (wnd_files)
     wnd_files->close();

  for (int i = 0; i < tracks.size(); i++)
      delete tracks[i];
 
  delete table_container;
  
  delete mixer_window;
  delete wnd_tracks;
  
  delete w_timeline;
  
  delete master_track;
   
  delete wnd_files;
  
  if (mixbuf_stream)
     {
      Pa_CloseStream (mixbuf_stream);	
      mixbuf_stream = 0;
     }
}


void CProject::save_project()
{
  QString output;

  QXmlStreamWriter stream (&output);
  stream.setAutoFormatting (true);

  stream.writeStartDocument();
  
  stream.writeStartElement("document");
  
  stream.writeAttribute ("bpm", QString::number (settings.bpm));
  stream.writeAttribute ("samplerate", QString::number (settings.samplerate));
  stream.writeAttribute ("panner", QString::number (settings.panner));
             
  stream.writeTextElement ("comments", comments); //project comments
         

  stream.writeStartElement ("mastertrack");
  
  stream.writeAttribute ("volume_left", QString::number (master_track->volume_left));
  stream.writeAttribute ("volume_right", QString::number (master_track->volume_right));
  
  stream.writeAttribute ("linked_channels", QString::number (master_track->linked_channels));
          
  
  stream.writeEndElement(); 
  

  stream.writeStartElement ("tracks");
  
  
  foreach (CTrack *t, tracks) 
          {
           stream.writeStartElement ("track");
           stream.writeAttribute ("channels", QString::number (t->channels));
           stream.writeAttribute ("volume", QString::number (t->volume));
           stream.writeAttribute ("pan", QString::number (t->pan));
           stream.writeAttribute ("mute", QString::number (t->mute));
           stream.writeAttribute ("solo", QString::number (t->solo));
           stream.writeAttribute ("arm", QString::number (t->arm));
           stream.writeAttribute ("monitor_input", QString::number (t->monitor_input));

           stream.writeAttribute ("type", t->track_type);
           stream.writeAttribute ("name", t->track_name);
    
           stream.writeTextElement ("comments", t->track_comment);
        
           if (t->track_type == "wav")
              { 
               foreach (CClip *clip, t->clips) 
                       {
                        CWaveClip *pc = (CWaveClip*) clip;
                        
                        stream.writeStartElement ("clip");
           
                        stream.writeAttribute ("filename", pc->filename);
                        stream.writeAttribute ("position_frames", QString::number(pc->position_frames));
                        stream.writeAttribute ("type", pc->type);
                        stream.writeAttribute ("name", pc->name);
                        stream.writeAttribute ("muted", QString::number (pc->muted));
                        stream.writeAttribute ("selected", QString::number (pc->selected));
                        stream.writeAttribute ("offset_frames", QString::number (pc->offset_frames));
                        stream.writeAttribute ("length_frames", QString::number (pc->length_frames));
                        stream.writeAttribute ("playback_rate", QString::number (pc->playback_rate));
                        
                        stream.writeEndElement(); 
                       }
              }
              
           
                   foreach (AFx *fx, t->fxrack.effects) 
                       {
                        stream.writeStartElement ("fx");
                        stream.writeAttribute ("classname", fx->classname);
                        stream.writeAttribute ("bypass", QString::number (fx->bypass));
                        stream.writeCharacters (fx->save_params_to_string());
                        stream.writeEndElement(); 
                       }   
                
        
           stream.writeEndElement(); 
          }
  

  stream.writeEndElement(); //tracks
    
  stream.writeEndElement(); //document
    
  stream.writeEndDocument();

  
  qstring_save (paths.fname_project, output);
}


void CProject::load_project()
{
 
  QString fname = paths.fname_project;

 // qDebug() << "CProject::load_project() " << fname;
  
  CTrack *t = 0;
  
  QString temp = qstring_load (fname);
  QXmlStreamReader xml (temp);
  
  while (! xml.atEnd())
        {
         xml.readNext();

         QString tag_name = xml.name().toString().toLower();

         if (xml.isStartElement() && tag_name == "document")
            {
             settings.bpm = get_value_with_default (xml.attributes().value ("bpm"), 120);
             settings.samplerate = get_value_with_default (xml.attributes().value ("samplerate"), 44100);
             settings.panner = get_value_with_default (xml.attributes().value ("panner"), 0);
             
             global_samplerate = settings.samplerate;
            }

            
         if (tag_name == "tracks" && xml.isStartElement())
            {
                
                
            }

         if (tag_name == "comments" && xml.isStartElement())
            {
             if (t)
                t->track_comment = xml.readElementText();
             else
                 comments = xml.readElementText();;  
            }

         if (tag_name == "track" && xml.isEndElement())
            t = 0;
            

         if (tag_name == "mastertrack" && xml.isStartElement())
            {
             master_track->volume_left = get_value_with_default (xml.attributes().value ("volume_left"), 1.0f);
             master_track->volume_right = get_value_with_default (xml.attributes().value ("volume_right"), 1.0f);
             master_track->linked_channels = get_value_with_default (xml.attributes().value ("linked_channels"), 1.0f);
             master_track->update_strip();
            }


        if (tag_name == "track" && xml.isStartElement())
           {
            QString attr_type = xml.attributes().value ("type").toString();
                 
            if (attr_type == "wav")
               {
                int nchannels = get_value_with_default (xml.attributes().value ("channels"), 1);
                     
                t = new CWavTrack (this, nchannels);
                     
                t->track_name = get_value_with_default (xml.attributes().value ("name"), "default");
                t->track_type = "wav";
                     
                t->channels = nchannels;
                     
                t->pan = get_value_with_default (xml.attributes().value ("pan"), 0.5f);
                t->volume = get_value_with_default (xml.attributes().value ("volume"), 0.0f);
                t->mute = get_value_with_default (xml.attributes().value ("mute"), 0.0f);
                t->solo = get_value_with_default (xml.attributes().value ("solo"), 0.0f);
                t->arm = get_value_with_default (xml.attributes().value ("arm"), 0.0f);
                t->monitor_input = get_value_with_default (xml.attributes().value ("monitor_input"), 0.0f);
            
                tracks.append (t);
                     
                t->update_strip();

                track_add_strip_to_mixer_window (t);
              }
            }


          if (tag_name == "fx" && xml.isStartElement())
             {
              if (! t)
                  continue;
                  
             QString attr_classname = xml.attributes().value ("classname").toString();
                  
             AFx *fx = /*t->fxrack.*/avail_fx->classnames[attr_classname]();     
             
             if (fx)     
                {
                 fx->bypass = get_value_with_default (xml.attributes().value ("bypass"), 0.0f);
                 
                 //qDebug() << "XXXXXXXXXXXXXXXXXXX: " << xml.readElementText();
                 fx->load_params_from_string (xml.readElementText()); 

                 t->fxrack.add_entry_silent (fx, fx->bypass);

                 //qDebug() << fx->modulename << " added";
                } 

             }

          if (tag_name == "clip" && xml.isStartElement())
             {
              if (! t)
                 continue;
                    
                    
             QString attr_type = xml.attributes().value ("type").toString();
                                  
                 if (attr_type != t->track_type)
                    continue; //тип текущей дорожки должен быть равен типу текущего клипа!
                         
                 if (attr_type == "wav")                 
                    {
                     CWaveClip *clip = new CWaveClip (t);
                     
                     clip->filename = xml.attributes().value ("filename").toString();
                     
                     if (! clip->filename.isEmpty())
                        clip->file = files.wavs[clip->filename];
                     
                     clip->position_frames = get_value_with_default (xml.attributes().value ("position_frames"), 0);
                     
                     clip->offset_frames = get_value_with_default (xml.attributes().value ("offset_frames"), 0);
                     clip->length_frames = get_value_with_default (xml.attributes().value ("length_frames"), 1);
                     
                     clip->type = get_value_with_default (xml.attributes().value ("type"), "wav");
                     
                     clip->name = get_value_with_default (xml.attributes().value ("name"), "default");
                     clip->muted = get_value_with_default (xml.attributes().value ("muted"), 0);
                     clip->selected = get_value_with_default (xml.attributes().value ("selected"), 0);
                 
                     clip->playback_rate = get_value_with_default (xml.attributes().value ("playback_rate"), 1.0f);
                     
                     t->clips.append (clip);
                    }
            
            }

  if (xml.hasError())
     qDebug() << "xml parse error";

  } //cycle

//qDebug() << "tracks.size: " << tracks.size();


 

  qDebug() << "CProject::load_tracks() - end";


 for (int i = 0; i < tracks.size(); i++)
    {
     table_track_create (tracks[i]);
     
     CTrackTableWidget *w = (CTrackTableWidget*)tracks[i]->table_widget;
     w->update_track_table();
       
     //qDebug() << "clips count: " << tracks[i]->clips.size();
    }


  refresh_song_length();
  //frames_per_pixel = song_length_frames / w_timeline->w_tracks->width();
  
  //qDebug() << "{{{{{{{{{{{{{{{{{{{{{{{{{[ frames_per_pixel: " << frames_per_pixel;
}


CWavTrack::CWavTrack (CProject *prj, int nchannels): CTrack (prj, nchannels)
{
  track_type = "wav";
  hrecfile = 0;
  fb_recbuffer = 0;
  
  track_widget = new CWAVTrackWidget (this, 0);
}


CTrack::~CTrack()
{
//  qDebug() << "CTrack::~CTrack()  -1";

  for (int i = 0; i < clips.size(); i++)
     delete clips[i];
     
//  qDebug() << track_name;   
     
  //delete buffer;   
  
  delete fbtrack;   
      
     
     
  delete mixer_strip;   
     
  delete gb_track;   
  
  delete track_widget;
  
  //delete table_widget;
     
//  qDebug() << "CTrack::~CTrack()  -2";
   
}


CProject::CProject()
{
  qDebug() << "CProject::CProject()";

  update_pos_slider = false;
  recording = false;

  wnd_files = 0;
  lw_wavclips = 0;
  lw_wavs = 0;
  cursor_frames = 0;
  
  song_length_frames = 0;
  
  mixbuf_window_start_frames_one = 0;
  mixbuf_window_length_frames_one = buffer_size_frames * buffer_size_frames_multiplier;
 
 
  tracks_window_start_frames = 0;
  tracks_window_length_frames = buffer_size_frames * buffer_size_frames_multiplier;
  tracks_window_inner_offset = 0;

  mixbuf_stream = 0;;
 
  master_track = new CMasterTrack (this);
   
  create_widgets();
  
  //mixer_window->show();
  
  connect (&gui_updater_timer, SIGNAL(timeout()), this, SLOT(gui_updater_timer_timeout()));
  gui_updater_timer.setInterval (100);
}


void CProject::bt_transport_play_click()
{
 // mixbuf_one();
  mixbuf_play();

}


void CProject::bt_transport_stop_click()
{
  mixbuf_stop();


}
 
void CProject::bt_transport_rewind_click()
{


}

void CProject::bt_transport_forward_click()
{


}
 
 
void CProject::bt_transport_record_click()
{
  //fb_stereo_rec->allocate_interleaved();
    

  mixbuf_record();

}


void CProject::slider_position_valueChanged (int value)
{
//  qDebug() << "slider_position_valueChanged " << value;
  
  if (! update_pos_slider)
     {
      ted_position->setTime (frames_to_time (value, global_samplerate));
      cursor_frames = value;
      mixbuf_window_start_frames_one = value;
      tracks_window_start_frames = value;
     } 
  
//  qDebug() << "mixbuf_window_start_frames: " << mixbuf_window_start_frames;
  
}


void CProject::create_widgets()
{
  qDebug() << "CProject::create_widgets()";
 
  table_container = new QWidget;
  table_widget_layout = new QHBoxLayout;
  table_container->setLayout (table_widget_layout);

  
  table_scroll_area = new QScrollArea;
  table_scroll_area->setWidget (table_container);
   
  
  transport_simple = new QWidget; 

  QVBoxLayout *v_transport = new QVBoxLayout;
  transport_simple->setLayout (v_transport);

  slider_position = new QSlider (Qt::Horizontal);
  slider_position->setTracking (true);

  connect (slider_position, SIGNAL(valueChanged(int)), this, SLOT(slider_position_valueChanged (int)));  

  ted_position = new QTimeEdit;
  ted_position->setDisplayFormat (TIME_FMT);

  
  v_transport->addWidget (ted_position);
  v_transport->addWidget (slider_position);
  
  
  QHBoxLayout *h_transport_buttons = new QHBoxLayout;
  v_transport->addLayout (h_transport_buttons);
  
  QPushButton *bt_rewind = new QPushButton ("←");
  QPushButton *bt_play = new QPushButton (">");
  QPushButton *bt_stop = new QPushButton ("∎");
  QPushButton *bt_record = new QPushButton ("*");
  QPushButton *bt_forward = new QPushButton ("→");
    
  h_transport_buttons->addWidget (bt_rewind); 
  h_transport_buttons->addWidget (bt_play); 
  h_transport_buttons->addWidget (bt_stop); 
  h_transport_buttons->addWidget (bt_record); 
  h_transport_buttons->addWidget (bt_forward); 
  
  connect (bt_rewind, SIGNAL(clicked()), this, SLOT(bt_transport_rewind_click()));  
  connect (bt_play, SIGNAL(clicked()), this, SLOT(bt_transport_play_click()));  
  connect (bt_stop, SIGNAL(clicked()), this, SLOT(bt_transport_stop_click()));  
  connect (bt_record, SIGNAL(clicked()), this, SLOT(bt_transport_record_click()));  
  connect (bt_forward, SIGNAL(clicked()), this, SLOT(bt_transport_forward_click()));  
  
  
  files_window_create();
  tracks_window_create();
  
  mixer_window = new CMixerWindow (this);
  
  w_timeline = new CTimeLine (this);
  
}


void CProject::refresh_song_length()
{
  qDebug() << "CProject::refresh_song_length()";
  
  song_length_frames = 0;
  
  for (int i = 0; i < tracks.size(); i++)
      {
       CTrack *t = tracks[i];
       if (t->mute)
          continue;
          
       if (t->clips.size() == 0)
          continue;   
          
       foreach (CClip *clip, tracks[i]->clips) 
               {
                if (! clip->muted)
                   {
                    size_t t = clip->position_frames + clip->length_frames;
                    if (t > song_length_frames)
                        song_length_frames = t; 
                   }    
               }
      }
      
      
   //иначе не будет записывать   
   if (song_length_frames == 0)   
      song_length_frames = std::numeric_limits<int>::max();

  slider_position->setMinimum (0);
  slider_position->setMaximum (song_length_frames);

//  song_length_frames += tracks_window_length_frames;
  
  if (cursor_frames >= song_length_frames)
     cursor_frames = song_length_frames - tracks_window_length_frames;
     
  w_timeline->sb_timeline->setMaximum (song_length_frames);   
     
 qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx";
 qDebug() << "song_length_frames: " << song_length_frames;
 qDebug() << "XXXXXXXXXXXXXXXXXXXXXXXXXXXX";   
}



void CProject::update_table (bool sort)
{
  for (int i = 0; i < tracks.size(); i++)
      {
       CTrackTableWidget *w = (CTrackTableWidget*)tracks[i]->table_widget;
       if (w)
          w->update_track_table (sort);
     }
     
  refresh_song_length();  
}


void CProjectManager::project_save()
{
  if (project)
     project->save_project();

}


CWaveClip::CWaveClip (CTrack *ptr): CClip (ptr)
{
  //qDebug() << "CWaveClip::CWaveClip()";
  type = "wav";
  file = 0;
  filename = "";
}


//глюконат
void CProject::table_itemChanged (QTableWidgetItem *item)
{

  CTrackTableWidget* w = dynamic_cast<CTrackTableWidget*>(sender());
  
  if (! w->p_track)
     return;

  if (item->text().isNull())
     return;

  size_t frames = timestring_to_frames (item->text());
  
  w->p_track->clips[item->row()]->position_frames = frames;
}


void CProject::table_itemActivated (QTableWidgetItem *item)
{
  table_clip_props();  
}


void CProject::table_track_create (CTrack *t)
{
  if (! t)
    return;
    
  qDebug() << "CProject::table_track_create: " << t->track_name; 

  t->gb_track = new QGroupBox (t->track_name);
  QVBoxLayout *v_track = new QVBoxLayout;
  t->gb_track->setLayout (v_track);
   
  CTrackTableWidget *table = new CTrackTableWidget;
  table->p_track = t;
  t->table_widget = table;

  table->setSelectionBehavior (QAbstractItemView::SelectRows);
  table->setSizePolicy (QSizePolicy::Expanding,QSizePolicy::Expanding);
  table->setEditTriggers (QAbstractItemView::NoEditTriggers); 
      
 // QAbstractItemDelegate *delegate = new QTimeEditItemDelegate;
 // table->setItemDelegateForColumn (0, delegate);

  table->setRowCount (t->clips.size());
  table->setColumnCount (2);

  QStringList table_header_labels;
  
  table_header_labels.append (tr ("Position"));
  table_header_labels.append (tr ("Clip"));

  table->setHorizontalHeaderLabels (table_header_labels);
  /*
  int row = 0;

  foreach (CClip *clip, t->clips) 
          {
           table->setItem (row, 0, new QTableWidgetItem (frames_to_time_str (clip->position_frames, global_samplerate)));
           table->setItem (row, 1, new QTableWidgetItem (clip->name));
           row++;
          }

  table->resizeColumnsToContents();
*/


//лучше не надо этот редактор!
//  connect (table, SIGNAL(itemChanged (QTableWidgetItem *)),
  //        this, SLOT(table_itemChanged(QTableWidgetItem *)));

  connect (table, SIGNAL(itemActivated (QTableWidgetItem *)),
          this, SLOT(table_itemActivated(QTableWidgetItem *)));

  v_track->addWidget (table);
  
  table_widget_layout->addWidget (t->gb_track);
  
  //QGraphicsProxyWidget *proxy = table_scene->addWidget (gb_track);
  //proxy->setPos (index * gb_track->width(), 1);
  
}


void CWaveClip::call_ui()
{
  if (! file)
     return;

  QDialog dialog;
  
  dialog.setWindowTitle (tr ("Clip properties"));

  
  QVBoxLayout *v_box = new QVBoxLayout;
  dialog.setLayout (v_box);

   QLabel *l_clipname = new QLabel (tr ("Clip name: ") + name);
   QLabel *l_type = new QLabel (tr ("Type: ") + type);
   QLabel *l_fname = new QLabel (tr ("Linked to file: ") + filename);
   QLabel *l_channels = new QLabel (tr ("Channels: ") + QString::number (file->fb->channels));

   v_box->addWidget (l_clipname);
   v_box->addWidget (l_type);
   v_box->addWidget (l_fname);
   v_box->addWidget (l_channels);
    
   QHBoxLayout *h_position = new QHBoxLayout;
   
   QLabel *l_position = new QLabel (tr ("Position at track"));
   QTimeEdit *ed_position = new QTimeEdit (frames_to_time (position_frames, global_samplerate));
   ed_position->setDisplayFormat (TIME_FMT);

   h_position->addWidget (l_position);
   h_position->addWidget (ed_position);

   v_box->addLayout (h_position);
  
   QHBoxLayout *h_offset = new QHBoxLayout;
   
   QLabel *l_offset = new QLabel (tr ("Start offset"));
   QTimeEdit *ed_offset = new QTimeEdit (frames_to_time (offset_frames, global_samplerate));
   ed_offset->setDisplayFormat (TIME_FMT);
  
   h_offset->addWidget (l_offset);
   h_offset->addWidget (ed_offset);

   v_box->addLayout (h_offset);

   QHBoxLayout *h_length = new QHBoxLayout;
   
   QLabel *l_length = new QLabel (tr ("Clip length"));
   QTimeEdit *ed_length = new QTimeEdit (frames_to_time (length_frames, global_samplerate));
   ed_length->setDisplayFormat (TIME_FMT);

   h_length->addWidget (l_length);
   h_length->addWidget (ed_length);

   v_box->addLayout (h_length);


   QHBoxLayout *h_playback_rate = new QHBoxLayout;

   QLabel *l_playback_rate = new QLabel ("Playback rate");
   QDoubleSpinBox *dsb_playback_rate = new QDoubleSpinBox;

   dsb_playback_rate->setDecimals (2); 
   dsb_playback_rate->setRange (0, 10.0);
   dsb_playback_rate->setSingleStep (0.01);
   dsb_playback_rate->setValue (playback_rate);

   h_playback_rate->addWidget (dsb_playback_rate);
   h_playback_rate->addWidget (l_playback_rate);
   
   v_box->addLayout (h_playback_rate);

   QCheckBox *cb_mute = new QCheckBox (tr ("Muted"));
   cb_mute->setChecked (muted);

   v_box->addWidget (cb_mute);

   QPushButton *bt_done = new QPushButton (tr ("Done"), &dialog);
   bt_done->setDefault (true);
   v_box->addWidget (bt_done);
   
   connect (bt_done, SIGNAL(clicked()), &dialog, SLOT(accept()));

   if (dialog.exec())
      {
       size_t new_position = timestring_to_frames (ed_position->text());
       size_t new_offset = timestring_to_frames (ed_offset->text());
       size_t new_length = timestring_to_frames (ed_length->text());
                            
       if (new_offset < file->fb->length_frames)       
          offset_frames = new_offset;
       
       //проверить это!
       size_t new_end = offset_frames + new_length;
       
       if (new_end > file->fb->length_frames)       
          new_end = file->fb->length_frames;
          
       length_frames = new_end - offset_frames;   
                
       position_frames = new_position;       
       muted = cb_mute->isChecked();
       
       p_track->p_project->refresh_song_length();
       
       playback_rate = dsb_playback_rate->value();
      }   
      
   dialog.close();   
}


void CProject::table_cellSelected (int row, int col)
{
//  QTableWidget* t = dynamic_cast<QTableWidget*>(sender());


 // QTableWidgetItem *item = t->item (row, 0);
  
  //qDebug() << item->text();

}


void QTimeEditItemDelegate::setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const 
{
  QTimeEdit *ed = (QTimeEdit*) editor;
  
  QVariant data = ed->text();
  
  model->setData (index, data, Qt::EditRole);
  model->submit();
}


QWidget* QTimeEditItemDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem &option, const QModelIndex & index) const
{
  Q_UNUSED (option);
  
  QTimeEdit *ed = new QTimeEdit (parent);
  ed->setDisplayFormat (TIME_FMT);
  
  return ed;
}


void CProject::table_clip_props()
{
  QWidget *w = table_container->focusWidget();

  if (! w)
      return;
      
  if (typeid(*w) == typeid(CTrackTableWidget)) 
     {
      CTrackTableWidget *p = (CTrackTableWidget*)w;
     
      int row = p->currentItem()->row();
     
      p->p_track->clips[row]->call_ui();
      p->update_track_table(); 
      refresh_song_length();
     }
}


void CTrackTableWidget::update_track_table (bool sort)
{
  if (! p_track)
     return;

  if (sort)
     qStableSort (p_track->clips.begin(), p_track->clips.end(), comp_clips);

  clearContents();
  setRowCount (p_track->clips.size());

  int row = 0;

  foreach (CClip *clip, p_track->clips) 
          {
           //qDebug() << "clip->name: " << clip->name;
           //qDebug() << "clip->position_frames: " << clip->position_frames;
           //qDebug() << "frames_to_time_str (clip->position_frames, global_samplerate): " << frames_to_time_str (clip->position_frames, global_samplerate);
          
           setItem (row, 0, new QTableWidgetItem (frames_to_time_str (clip->position_frames, global_samplerate)));
           setItem (row, 1, new QTableWidgetItem (clip->name));
           row++;
          }

  resizeColumnsToContents();
}


void CTrackTableWidget::keyPressEvent (QKeyEvent *event)
{
  if (event->key() == Qt::Key_Delete)
     {
     
      int row = currentItem()->row();
     
      delete p_track->clips[row];
      p_track->clips.removeAt (row);
      update_track_table();    
     }

  QTableWidget::keyPressEvent (event);
}




void CProject::list_tracks()
{
  for (int i = 0; i < tracks.size(); i++)
      {
       qDebug() << "track name: " << tracks[i]->track_name;
       
       foreach (CClip *clip, tracks[i]->clips) 
          {
           qDebug() << "clip name: " << clip->name;
           qDebug() << "clip frames: " << clip->position_frames;
          }
     }
}




CTrack::CTrack (CProject *prj, int nchannels)
{
  qDebug() << "CTrack::CTrack ";

  focused = false;

  p_project = prj;
  channels = nchannels;
  //buffer_offset_frames = 0;
  
  monitor_input = false;
  
  record_insertion_pos_frames = 0;
  
  //master = is_master;
  
  pan = 0.5;
  volume = 1.0f;
  /*
  volume_left = 1.0f;
  volume_right = 1.0f;
  linked_channels = true;
  */
  mute = false;
  solo = false;
  arm = false;
  
 // buffer_length_frames = buffer_size_frames * buffer_size_frames_multiplier;
  
  //buffer = new float [buffer_length_frames * channels];

  fbtrack = new CFloatBuffer (buffer_size_frames * buffer_size_frames_multiplier, channels);

  track_name = tr ("Default");
  
  
  mixer_strip = new CMixerStrip (this);
  
  //qDebug() << "buffer_length_frames: " << buffer_length_frames;
 
}



void CProject::track_insert_new()
{
// qDebug() << "CProject::track_insert_new()";

 QWidget *w = table_container->focusWidget();

/*
 int row = 0;

  if (w)
  if (typeid(*w) == typeid(CTrackTableWidget)) 
    {
     CTrackTableWidget *p = (CTrackTableWidget*)w;
     row = p->currentItem()->row();
     }
  */   
  
  bool ok;
  int channels = QInputDialog::getInt (0, "?",
                                      tr ("How manu channels?"), 1, 1, 2, 1, &ok);
                                          
  if (! ok || channels < 0)
     return;
  
  
  CWavTrack *track = new CWavTrack (this, channels);
     
  //tracks.insert (row, track);

  tracks.append (track);

  table_track_create (track);
     
  CTrackTableWidget *tw = (CTrackTableWidget*)track->table_widget;
  tw->update_track_table();
  track->update_strip();
  
  track_add_strip_to_mixer_window (track);
}


void CProject::track_delete (int track_number)
{
  QWidget *w = table_container->focusWidget();

 int row = 0;

  if (w)
  if (typeid(*w) == typeid(CTrackTableWidget)) 
    {
     CTrackTableWidget *p = (CTrackTableWidget*)w;
     
     }
 

}


void CProject::track_swap (int track_number1, int track_number2)
{


}  


//start_pos_frames - начало окна на дорожке
size_t CWavTrack::render_portion (size_t start_pos_frames, size_t window_length_frames)
{
  size_t end_pos_frames = start_pos_frames + window_length_frames; //конец окна на дорожке

  int clips_count = clips.size();
 
  fbtrack->settozero(); 

/*

сюда же вставить проверку, находятся ли первый и последний клипы в пределах курсора.
если нет, то вообще не рендерим больше дорожку

*/
     
  for (int i = 0; i < clips_count; i++)
      {
       CWaveClip *clip = (CWaveClip*)clips[i];

       if (clip->muted)
          continue;
      
       //qDebug() << "***** clip: " << clip->name << " **** " << clip->length_frames;
       
      // qDebug() << "clip: " << clip->name;
       //qDebug() << "clip len: " << clip->length_frames;
       
       //check is clip in range?
       
       size_t clip_start = clip->position_frames; //начало клипа на дорожке, положение
       size_t clip_end = clip_start + clip->length_frames;//конец клипа на дорожке, положение
       
       int clip_in_window = 0; //0 - not, 1 - window len <= clip, 2 - window len => clip
       
       if (window_length_frames >= clip->length_frames)
          {
           if ((start_pos_frames <= clip_start && clip_start <= end_pos_frames) ||
               (start_pos_frames <= clip_end && clip_end <= end_pos_frames)) 
              clip_in_window = 2; 
          }
       else
           {
            if ((clip_start <= start_pos_frames && start_pos_frames <= clip_end) ||
                (clip_start <= end_pos_frames && end_pos_frames <= clip_end))
               clip_in_window = 1;    
            }  
       
     //  qDebug() << clip_in_window;
      
       if (clip_in_window == 0)
          continue;
                
//       qDebug() << "clip: " << clip->name << " is in window";  
        
       //теперь вычислить отрезок клипа, входящий в окно
       // именно смещение клипа относительно начала клипа и смещение относительно конца клипа
        
       size_t clip_data_start = 0;  //где в клипе начало попадающего в окно отрезка? относительно начала клипа
       size_t clip_insertion_pos = 0; //куда вставлять в буфер дорожки?
       size_t clip_data_length = 0;
        
       if (clip_in_window == 1)
          {
         //  qDebug() << clip->name << " clip_in_window == 1";
           
           if (start_pos_frames <= clip_start)
             {
           //   qDebug() << "x1";
              clip_insertion_pos = clip_start - start_pos_frames;
              clip_data_start = 0;
              clip_data_length = end_pos_frames - clip_start;
             }
           else   
               if (start_pos_frames > clip_start)
                  {
                   //qDebug() << "x2";

                   clip_insertion_pos = 0; //по границе начала окна
                   clip_data_start = start_pos_frames - clip_start;
                   
                   //qDebug() << "clip_data_start: " << clip_data_start;
                   //qDebug() << "clip_end: " << clip_end;
                   //qDebug() << "end_pos_frames: " << end_pos_frames;
            
                   if (clip_end >= end_pos_frames)
                       clip_data_length = window_length_frames;
                   else
                      {
                      //qDebug() << "x3";   
                      // clip_data_length = end_pos_frames - clip_start;
                        clip_data_length = clip_end - start_pos_frames;
                      
                       
                      } 
                       
                   qDebug() << "!!! clip_data_length: " << clip_data_length;
                
                 }
           }
        
        if (clip_in_window == 2)
           {
    //       qDebug() << "clip_in_window == 2";
          
            if (start_pos_frames <= clip_start)
               {
                clip_insertion_pos = clip_start - start_pos_frames;
                clip_data_start = 0;
                
                if (clip_start >= start_pos_frames && clip_end <= end_pos_frames)
                   clip_data_length = clip->length_frames;
                else   
                   clip_data_length = end_pos_frames - clip_start;
               }
            else   
                if (start_pos_frames > clip_start)
                   {
                    clip_insertion_pos = 0; //по границе начала окна
                    clip_data_start = start_pos_frames - clip_start;
          
                    if (clip_start >= start_pos_frames && clip_end >= end_pos_frames)
                        clip_data_length = clip->length_frames;
                    else   
                        clip_data_length = clip_end - start_pos_frames;
                   }
            }
          
        //ошибка с clip_data_length, может быть больше, чем действительно есть?  
          
      //  qDebug() << "clip_data_start: " << clip_data_start;
//        qDebug() << "clip_data_length: " << clip_data_length;
  //      qDebug() << "clip_insertion_pos: " << clip_insertion_pos;
        
      
        size_t extoffs = (clip->offset_frames + clip_data_start) * clip->playback_rate;
       
           //ошибка в вычислении clip_insertion_pos???     
           //тут может быть вылет!!!  
        
        if (clip->playback_rate == 1.0f && clip->file)
          {
           clip->file->fb->copy_to_pos (fbtrack, extoffs, clip_data_length, clip_insertion_pos);
          } 
        else 
        if (clip->file)
            {
             qDebug() << "clip->playback_rate != 1.0f";
            
             clip->file->fb->copy_to_pos_with_rate (fbtrack, extoffs, 
                              clip_data_length, clip_insertion_pos, clip->playback_rate);
            
             qDebug() << "clip->playback_rate != 1.0f";
            }   
       }   
       
       
  return end_pos_frames;
}  


size_t CProject::tracks_render_next()
{
  qDebug() << "CProject::tracks_render_next()  - 1";
  
 // qDebug() << "tracks_window_start_frames: " << tracks_window_start_frames << " tracks_window_length_frames: " << tracks_window_length_frames;
  //qDebug() << "tracks_window_start time: " << frames_to_time_str (tracks_window_start_frames, global_samplerate) <<
    //          "tracks_window_length time: " << frames_to_time_str (tracks_window_length_frames, global_samplerate);
 
  if (tracks_window_start_frames >= song_length_frames - tracks_window_length_frames)
     {
      qDebug() << "tracks_window_start_frames >= song_length_frames";
      return song_length_frames;
     } 
       
       
  for (int i = 0; i < tracks.size(); i++)
      {
       CTrack *p_track = tracks[i];
       p_track->render_portion (tracks_window_start_frames, tracks_window_length_frames);  
       p_track->fbtrack->pbuffer_reset();
      }
 
  
  tracks_window_start_frames += tracks_window_length_frames;
    
  qDebug() << "CProject::tracks_render_next()  - 2";
  
     
  return tracks_window_start_frames;   
}

/*
если окно больше, чем осталось до конца песни, то идет END OF SONG
ИСПРАВИТЬ!
*/


int CProject::mixbuf_render_next (int rendering_mode, const void *inpbuf)
{
  //qDebug() << "CProject::mixbuf_render_next() - 1";

  float **p_input_buf = (float **)inpbuf;

  size_t ret = 0;
  
  if (tracks_window_inner_offset == 0)
     ret = tracks_render_next();
     
  if (ret >= song_length_frames) 
     {
      qDebug() << "!!!!!!!!!!!!!!!!!!!!! END OF SONG .,.,., !!!!!!!!!!!!!!!!!";
      return -1;
     }

  bool has_solo = false;

  for (int i = 0; i < tracks.size(); i++)
      {
       if (tracks[i]->solo)
          {
           has_solo = true;
           break;
          } 
      }  
  
  master_track->fb->settozero();

  for (int i = 0; i < tracks.size(); i++)
      {
       CTrack *p_track = tracks[i];
  
       if (p_track->mute)
          continue;
  
       if (has_solo && ! p_track->solo)
          continue;

       /*
        if monitoring, mix with input:
       */

        if (p_track->monitor_input) //учесть специфику при записи в моно!
           {
            size_t frame = 0;
         
            while (frame < buffer_size_frames)
                  {
                   p_track->fbtrack->pbuffer[0][frame] += p_input_buf[0][frame];
                   
                   if (p_track->channels == 2)       
                      p_track->fbtrack->pbuffer[1][frame] += p_input_buf[1][frame];
                      
                   frame++;
                  }
           }
     
           

//INSERTS
   
        for (int i = 0; i < p_track->fxrack.effects.size(); i++)
            {
             if (! p_track->fxrack.effects[i]->bypass)
                {
                 p_track->fxrack.effects[i]->channels = p_track->channels;
                 p_track->fxrack.effects[i]->samplerate = settings.samplerate;
                     
                 p_track->fxrack.effects[i]->execute (p_track->fbtrack->pbuffer, p_track->fbtrack->pbuffer, buffer_size_frames);
                }
             }



//SENDS TO BUSES


//END SENDS TO BUSES
 ////
             
        float maxl = 0.0f;
        float maxr = 0.0f;
            
        float sqr_sum_l = 0.0f;
        float sqr_sum_r = 0.0f;
            
        size_t frame = 0;
            
        //mix trackbuffer with mastertrack    
        while (frame < buffer_size_frames)
              {
               float panl = 0.0;
               float panr = 0.0;
            
               if (settings.panner == 0)
                  pan_linear0 (panl, panr, p_track->pan);
               else
               if (settings.panner == 1)
                  pan_linear6 (panl, panr, p_track->pan);
               else
               if (settings.panner == 2)
                   pan_sqrt (panl, panr, p_track->pan);
               else
               if (settings.panner == 3)
                  pan_sincos (panl, panr, p_track->pan);

               if (p_track->channels == 2)
                  { 
                   float l = p_track->fbtrack->pbuffer[0][frame] * panl * p_track->volume;
                   float r = p_track->fbtrack->pbuffer[1][frame] * panr * p_track->volume;
                   
                   master_track->fb->buffer[0][frame] += l;
    
                   if (float_less_than (maxl, l))
                       maxl = l;

                   sqr_sum_l += l * l;
                 
                   master_track->fb->buffer[1][frame] += r;
    
                  if (float_less_than (maxr, r))
                    maxr = r;

                  sqr_sum_r += r * r;
                  }
               else
               if (p_track->channels == 1)
                  { 
                   float l = p_track->fbtrack->pbuffer[0][frame] * panl * p_track->volume;
                   float r = p_track->fbtrack->pbuffer[0][frame] * panr * p_track->volume;
                   
                   master_track->fb->buffer[0][frame] += l;
    
                   if (float_less_than (maxl, l))
                       maxl = l;

                   sqr_sum_l += l * l;
                 
                   master_track->fb->buffer[1][frame] += r;
    
                   if (float_less_than (maxr, r))
                     maxr = r;

                   sqr_sum_r += r * r;
                  }
                  
                    
               frame++;
              }
                   
//MIX AUX BUSES WITH MASTERTRACK

//END MIX AUX BUSES 
                   
          
      float srms_l = sqrt (sqr_sum_l / buffer_size_frames);             
      float srms_r = sqrt (sqr_sum_r / buffer_size_frames);             

      p_track->mixer_strip->level_meter->muted = p_track->mute;
     
      p_track->mixer_strip->level_meter->pl = maxl;
      p_track->mixer_strip->level_meter->pr = maxr;
                       
      p_track->mixer_strip->rms_meter->pl = srms_l;
      p_track->mixer_strip->rms_meter->pr = srms_r;
      
      
      p_track->fbtrack->pbuffer_inc (buffer_size_frames);
     }


   //RECORDING
   
   if (recording)
      {
       for (int i = 0; i < tracks.size(); i++)
           {
            CWavTrack *p_track = (CWavTrack *)tracks[i];
            if (p_track->arm)
               p_track->record_iteration (inpbuf, buffer_size_frames);
           }     
      } 

   

  //MASTER TRACK

  //DSP, then
   
  
     float maxl = 0.0f;
     float maxr = 0.0f;
            
     float sqr_sum_l = 0.0f;
     float sqr_sum_r = 0.0f;
            
      
     size_t frame = 0;      
            
     while (frame < buffer_size_frames)
           {
            master_track->fb->buffer[0][frame] = master_track->fb->buffer[0][frame] * master_track->volume_left;  
    
            if (float_less_than (maxl, master_track->fb->buffer[0][frame]))
               maxl = master_track->fb->buffer[0][frame];

            sqr_sum_l += master_track->fb->buffer[0][frame] * master_track->fb->buffer[0][frame];
                     
                    
            master_track->fb->buffer[1][frame] = master_track->fb->buffer[1][frame] * master_track->volume_right;  
    
            if (float_less_than (maxr, master_track->fb->buffer[1][frame]))
               maxr = master_track->fb->buffer[1][frame];

            sqr_sum_r += master_track->fb->buffer[1][frame] * master_track->fb->buffer[1][frame];
                  
            frame++;
           }
                   
          
      float srms_l = sqrt (sqr_sum_l / buffer_size_frames);             
      float srms_r = sqrt (sqr_sum_r / buffer_size_frames);             
     
      master_track->mixer_strip->level_meter->pl = maxl;
      master_track->mixer_strip->level_meter->pr = maxr;
                       
      master_track->mixer_strip->rms_meter->pl = srms_l;
      master_track->mixer_strip->rms_meter->pr = srms_r;


  //and go to next iteration

  cursor_frames += buffer_size_frames;
  tracks_window_inner_offset += buffer_size_frames;
 
  if (tracks_window_inner_offset >= tracks_window_length_frames)
     tracks_window_inner_offset = 0;

  //qDebug() << "CProject::mixbuf_render_next() - 2";


  return tracks_window_inner_offset;
}



int mixbuf_pa_stream_callback (const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
  //qDebug() << "mixbuf_pa_stream_callback  -1";   
     
  CProject *p_project = (CProject*) userData;

  float** pchannels = (float **)output;

  int r = p_project->mixbuf_render_next (0, input);
 
  if (r == -1)
     {
      qDebug() << "?????????????????????????";
      return paComplete;
     } 
    
    
  if (p_project->cursor_frames >= p_project->song_length_frames)   
     {
      qDebug() << "!!!!!!!!!!!!!!!!!!! END OF SONG !!!!!!!!!!!!!!!!!!!!!";
      return paComplete;
     } 

  memcpy (pchannels[0], p_project->master_track->fb->buffer[0], 
          frameCount * sizeof (float));
 
  memcpy (pchannels[1], p_project->master_track->fb->buffer[1], 
          frameCount * sizeof (float));



 // qDebug() << "mixbuf_pa_stream_callback  -2";   
    
  return paContinue; 	
}


void CProject::mixbuf_record()
{
  recording = true;

  //if (fb_stereo_rec)
    // delete fb_stereo_rec;
  

  //найти, на какую дорожку пишем
  //открываем файл дорожки на запись
  
  
  bool armed = false;
  
  for (int i = 0; i < tracks.size(); i++)
      {
       if (tracks[i]->arm)
          {
           armed = true;
           break;   
          }
       }   
  

  if (! armed)  
     {
      QMessageBox::warning (0, tr ("Oops!"), "Arm some track first", QMessageBox::Ok);
      return;
     }
  
  for (int i = 0; i < tracks.size(); i++)
      {
       CWavTrack *p_track = (CWavTrack*) tracks[i];
       if (p_track->arm)
          {
           p_track->record_start();
          
           p_track->record_insertion_pos_frames = cursor_frames;
           
          }
      }
  
  
  mixbuf_play();
}




void fnPaStreamFinishedCallback (void *userData)
{


}


void CProject::mixbuf_play()
{
  qDebug() << "CProject::mixbuf_play()  -1";

  tracks_window_inner_offset = 0;
  tracks_window_start_frames = cursor_frames;   
  
  if (mixbuf_stream)
     {
      Pa_CloseStream (mixbuf_stream);	
      mixbuf_stream = 0;
     }

   for (int i = 0; i < tracks.size(); i++)
        {
         tracks[i]->fxrack.reset_all_fx (settings.samplerate, tracks[i]->channels);
        }

   PaStreamParameters inputParameters;

   inputParameters.device = pa_device_id_in;
   inputParameters.channelCount = 2;
   
   inputParameters.sampleFormat = paFloat32;
   inputParameters.sampleFormat |= paNonInterleaved;
 
   
   inputParameters.suggestedLatency = Pa_GetDeviceInfo (inputParameters.device)->defaultLowOutputLatency;;
   inputParameters.hostApiSpecificStreamInfo = NULL;
  
   PaStreamParameters outputParameters;

   outputParameters.device = pa_device_id_out;
   outputParameters.channelCount = 2;
   
   outputParameters.sampleFormat = paFloat32;
   outputParameters.sampleFormat |= paNonInterleaved;
 
   outputParameters.suggestedLatency = Pa_GetDeviceInfo (outputParameters.device)->defaultLowOutputLatency;;
   outputParameters.hostApiSpecificStreamInfo = NULL;
  
   PaError err = Pa_OpenStream (&mixbuf_stream,
                               &inputParameters,
                               &outputParameters,
                               settings.samplerate,
	                           buffer_size_frames,
		                       paDitherOff,
		                       mixbuf_pa_stream_callback,
		                       this
	                          ); 	
   
  // qDebug() << "buffer_size_frames:" << buffer_size_frames;
   //qDebug() << "settings.samplerate:" << settings.samplerate;
   
  qDebug() << Pa_GetErrorText (err);
       
  if (err < 0)
     {
      qDebug() << (Pa_GetErrorText (err));
      mixbuf_stream = 0;
      return;
     }
     

  Pa_SetStreamFinishedCallback (mixbuf_stream, fnPaStreamFinishedCallback);
      
  err = Pa_StartStream (mixbuf_stream);
  qDebug() << Pa_GetErrorText (err);

  if (err < 0)
     {
      qDebug() << (Pa_GetErrorText (err));
      mixbuf_stream = 0;
     }

  gui_updater_timer.start();

  qDebug() << "CProject::mixbuf_play()  -2";
}



void CProject::mixbuf_stop()
{

  if (mixbuf_stream)
     {
      Pa_CloseStream (mixbuf_stream);	
      mixbuf_stream = 0;
     }
     
     
   if (recording)
      {
      
       recording = false;
       //close file[s], add to wavs, create clips, place the clips
       
       for (int i = 0; i < tracks.size(); i++)
          {       
           CWavTrack *p_track = (CWavTrack*) tracks[i];
           if (p_track->arm)
              {
               p_track->record_stop();
               refresh_song_length();
              }
          }          
 
        lw_wavs_refresh();        
      }
      

  gui_updater_timer.stop(); 
}


CClip::CClip (CTrack *ptr)
{
  p_track = ptr;
  playback_rate = 1.0f;
  muted = false;
  selected = false;
}


CMixerWindow::CMixerWindow (CProject *ptr)
{
  p_project = ptr;
  
  setWindowTitle (tr ("Mixer"));
  
  h_main = new QHBoxLayout;
  setLayout (h_main);

  channel_strips_container = new QWidget;
  h_channel_strips_container = new QHBoxLayout;
  channel_strips_container->setLayout (h_channel_strips_container);

  channel_strips_scroll = new QScrollArea;
  channel_strips_scroll->setWidgetResizable (true);
  channel_strips_scroll->setWidget (channel_strips_container);
   
  h_main->addWidget (channel_strips_scroll);
  h_main->addWidget (p_project->master_track->mixer_strip);
}


/*
int pan_float_to_int (float floatpan)
{  
  if (floatpan == 0.5)
   return 0;

  if (floatpan < 0.5)
   return (floatpan - 1) * 100;

  if (floatpan > 0.5)
     return floatpan * 100;
     
  return 0;   
}


float pan_int_to_float (int value) //ok
{
  //value: -100, 0, 100
  //0 - left, 0.5 - middle, 1 - right
   
   if (value < 0)
       {
        int v = (100 - value * -1);
        return get_fvalue (0.5f, v);
       }
   
   if (value > 0)
       return (get_fvalue (0.5f, value) + 0.5f);
 
 
  //if (value == 0)
   return 0.5f;
}
*/

void CProject::gui_updater_timer_timeout()
{
   update_pos_slider = true;
   
   slider_position->setValue (cursor_frames);
   ted_position->setTime (frames_to_time (cursor_frames, global_samplerate));
 
   for (int i = 0; i < tracks.size(); i++)
      {
       CTrack *t = tracks[i];
       
       if (! t->mute)
          {
           t->mixer_strip->level_meter->update();
           t->mixer_strip->rms_meter->update();
          }
      }  
     
   master_track->mixer_strip->level_meter->update();
   master_track->mixer_strip->rms_meter->update();
   
   update_pos_slider = false;
}  



void CTrack::update_strip()
{
 //qDebug() << "CTrack::update_strip(): " << track_name;
// qDebug() << "CTrack::volume: " << float2db (volume);

  mixer_strip->update_strip_controls = true;
  
  mixer_strip->setTitle (track_name);
  
  mixer_strip->dsb_vol->setValue (float2db (volume));
  mixer_strip->dsb_pan->setValue (pan);
  mixer_strip->cb_mute->setChecked (mute);
  mixer_strip->cb_solo->setChecked (solo);
  mixer_strip->cb_arm->setChecked (arm);
  mixer_strip->cb_monitor_input->setChecked (monitor_input);
  
  mixer_strip->update_strip_controls = false;
}


void CProject::track_add_strip_to_mixer_window (CTrack *t)
{
  mixer_window->h_channel_strips_container->addWidget (t->mixer_strip);

}


CMasterTrack::CMasterTrack (CProject *prj)
{
  qDebug() << "CMasterTrack::CMasterTrack ";

  track_name = tr ("Master");
  
  p_project = prj;
  
  channels = 2;  
    
  volume_left = 1.0f;
  volume_right = 1.0f;
  linked_channels = true;
  
  fb = new CFloatBuffer (buffer_size_frames, 2);  
  mixer_strip = new CMixerMasterStrip (this);
  
  update_strip();
} 


void CProject::mixdown_to_default()
{
  SNDFILE *file_mixdown_handle;

  int sndfile_format = 0;
  sndfile_format = sndfile_format | SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  int channels = 2;
  int samplerate = settings.samplerate;
  
  SF_INFO sf;

  sf.samplerate = samplerate;
  sf.channels = channels;
  sf.format = sndfile_format;
   
  if (! sf_format_check (&sf))
     {
      qDebug() << "! sf_format_check (&sf)";
      return;  
     }    

  slider_position->setValue (0); 
    
  tracks_window_inner_offset = 0;
  tracks_window_start_frames = 0;   
  
  for (int i = 0; i < tracks.size(); i++)
      {
       tracks[i]->fxrack.reset_all_fx (settings.samplerate, tracks[i]->channels);
      }

  
  QString tf = paths.output_dir + "/" + "out.wav";
  
  file_mixdown_handle = sf_open (tf.toUtf8().data(), SFM_WRITE, &sf);

  master_track->fb->allocate_interleaved();

  while (cursor_frames < (song_length_frames - buffer_size_frames))
        {
         if (mixbuf_render_next (RENDER_MODE_OFFLINE) == -1)
             break;
         
//          master_track->fb->allocate_interleaved();
          master_track->fb->fill_interleaved();             
          sf_writef_float (file_mixdown_handle, (float *)master_track->fb->buffer_interleaved, buffer_size_frames);
          memset (master_track->fb->buffer_interleaved, 0, master_track->fb->length_frames * channels * sizeof (float));
         }
    
  slider_position->setValue (0); 

  sf_close  (file_mixdown_handle);
  
  qDebug() << "RENDERED";
 
}




void CMasterTrack::update_strip()
{
  mixer_strip->update_strip_controls = true;
  mixer_strip->setTitle (track_name);
  
  mixer_strip->dsb_vol_l->setValue (float2db (volume_left));
  mixer_strip->dsb_vol_r->setValue (float2db (volume_right));
  mixer_strip->cb_linked_channels->setChecked (linked_channels);
  
  mixer_strip->update_strip_controls = false;
}


CMasterTrack::~CMasterTrack()
{
  delete fb;
}



void CMixerMasterStrip::cb_linked_channels_toggled (bool checked)
{
 if (update_strip_controls)
     return;

  p_track->linked_channels = checked;
}


void CMixerStrip::cb_mute_toggled (bool checked)
{
 if (update_strip_controls)
     return;

  p_track->mute = checked;
  
  p_track->p_project->refresh_song_length();
}


void CMixerStrip::cb_monitor_input_toggled (bool checked)
{
 if (update_strip_controls)
     return;

  p_track->monitor_input = checked;
  
}


void CMixerStrip::cb_solo_toggled (bool checked)
{
 if (update_strip_controls)
     return;

  p_track->solo = checked;
}


void CMixerStrip::cb_arm_toggled (bool checked)
{
  if (update_strip_controls)
     return;

  p_track->arm = checked;
  
  if (p_track->arm)
     {
      foreach (CTrack *track, p_track->p_project->tracks) 
              {
               track->arm = false;
               track->update_strip();
              }
     }  
     
  p_track->arm = checked;
  p_track->update_strip();
}


void CMixerStrip::call_track_properties()
{
  p_track->call_properties_wnd(); 
}


void CMixerStrip::call_track_inserts()
{
  if (! p_track->fxrack.inserts->isVisible())
     p_track->fxrack.inserts->show(); 
  else   
      p_track->fxrack.inserts->hide(); 
}


CMixerStrip::CMixerStrip (CTrack *ptrk, QWidget *parent): QGroupBox (parent)
{
  p_track = ptrk;
  
 qDebug() << "CMixerStrip::CMixerStrip (CTrack *ptrk): " << p_track->track_name;
  
  QString gbs = "CMixerStrip {"
    "border: 1px solid gray;"
    //"border-radius: 5px;"
    "margin-top: 1ex;} "
"CMixerStrip::title {"
    "subcontrol-origin: margin;"
    "subcontrol-position: top center;" 
    "padding: 0 3px;}";
    //"background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FFOECE, stop: 1 #FFFFFF);}";
  
  setStyleSheet (gbs);
 
  update_strip_controls = true;
  
  QVBoxLayout *v_main = new QVBoxLayout;
  setLayout (v_main);

  QPushButton *bt_properties = new QPushButton (tr ("Properties"));
  connect (bt_properties, SIGNAL(clicked()), this, SLOT(call_track_properties()));
  v_main->addWidget (bt_properties);
  
 
  QGroupBox *gb_controls = new QGroupBox (tr ("Record"));
  QVBoxLayout *v_controls = new QVBoxLayout;
  gb_controls->setLayout (v_controls);
  
  cb_arm = new QCheckBox (tr ("Arm"));
  v_controls->addWidget (cb_arm);
  
   cb_arm->setChecked (p_track->arm);
   connect (cb_arm, SIGNAL(toggled(bool)), this, SLOT(cb_arm_toggled(bool)));
 
  
   cb_monitor_input = new QCheckBox (tr ("Monitor input"));
   v_controls->addWidget (cb_monitor_input);
  
   cb_monitor_input->setChecked (p_track->monitor_input);
   connect (cb_monitor_input, SIGNAL(toggled(bool)), this, SLOT(cb_monitor_input_toggled(bool)));
  
   
  
  QGroupBox *gb_fx = new QGroupBox (tr ("FX"));
  QVBoxLayout *v_fx = new QVBoxLayout;
  gb_fx->setLayout (v_fx);
  
  QPushButton *bt_inserts = new QPushButton (tr ("Inserts"));
  connect (bt_inserts, SIGNAL(clicked()), this, SLOT(call_track_inserts()));
  
  v_fx->addWidget (bt_inserts);
  
  QPushButton *bt_sends = new QPushButton (tr ("Sends"));
  v_fx->addWidget (bt_sends);
  
  QGroupBox *gb_vol = new QGroupBox (tr ("Level"));
  QVBoxLayout *v_vol = new QVBoxLayout;
  gb_vol->setLayout (v_vol);

      dsb_vol = new QDoubleSpinBox;
      dsb_vol->setDecimals (2); 
      dsb_vol->setRange (-90, 6.0);
      dsb_vol->setSingleStep (0.01);
      dsb_vol->setValue (float2db (p_track->volume));
    
      connect (dsb_vol, SIGNAL(valueChanged(double)), this, SLOT(dsb_vol_valueChanged(double)));
 
      cb_mute = new QCheckBox (tr ("Mute"));
      cb_mute->setTristate (false);
  
      cb_mute->setChecked (p_track->mute);
      connect (cb_mute, SIGNAL(toggled(bool)), this, SLOT(cb_mute_toggled(bool)));
  
      cb_solo = new QCheckBox (tr ("Solo"));
      cb_solo->setTristate (false);
  
      cb_solo->setChecked (p_track->solo);
      connect (cb_solo, SIGNAL(toggled(bool)), this, SLOT(cb_solo_toggled(bool)));
   
      v_vol->addWidget (dsb_vol);
      v_vol->addWidget (cb_mute);
      v_vol->addWidget (cb_solo);
  
     
  
  QGroupBox *gb_pan = new QGroupBox (tr ("Panorama"));
      QVBoxLayout *v_pan = new QVBoxLayout;
      gb_pan->setLayout (v_pan);
   
      dsb_pan = new QDoubleSpinBox;
      dsb_pan->setDecimals (2); 
      dsb_pan->setRange (0.0, 1.0);
      dsb_pan->setSingleStep (0.01);
      dsb_pan->setValue (p_track->pan);

      connect (dsb_pan, SIGNAL(valueChanged(double)), this, SLOT(dsb_pan_valueChanged(double)));
 
      v_pan->addWidget (dsb_pan);
  

  QGroupBox *gb_meter = new QGroupBox (tr ("Meter"));
  QHBoxLayout *h_meter = new QHBoxLayout;
  gb_meter->setLayout (h_meter);
    
  level_meter = new CLevelMeter (32, 16, 128, "#ffaa00", "#ffaa7f");
  rms_meter = new CLevelMeter (32, 16, 128, "#aa557f", "#55557f");

  QVBoxLayout *v_level_meter = new QVBoxLayout;
  QLabel *l_level_meter = new QLabel (tr ("Peaks"));
  
  v_level_meter->addWidget (l_level_meter);
  v_level_meter->addWidget (level_meter);

  QVBoxLayout *v_rms_meter = new QVBoxLayout;
  QLabel *l_rms_meter = new QLabel (tr ("RMS"));
  
  v_rms_meter->addWidget (l_rms_meter);
  v_rms_meter->addWidget (rms_meter);


  h_meter->addLayout (v_level_meter);
  h_meter->addLayout (v_rms_meter);

   
  update_strip_controls = false;
  

  v_main->addWidget (gb_controls);
  
  v_main->addWidget (gb_fx);
  
  v_main->addWidget (gb_vol);
  
  v_main->addWidget (gb_pan);
     
  v_main->addWidget (gb_meter);
  
  setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
}



void CMixerStrip::dsb_pan_valueChanged (double d)
{
 if (update_strip_controls)
     return;
  
   //0 - left, 0.5 - middle, 1 - right
   
   p_track->pan = (float) d;
 
 //  qDebug() << "p_track->pan: " << p_track->pan;
}


void CMixerStrip::dsb_vol_valueChanged (double d)
{
 if (update_strip_controls)
     return;
   
  float f = 1.0f;
  
  if (d == 0.0)
     {
      p_track->volume = 1.0f;
      return;
     }

   f = db2lin (d);

   p_track->volume = f;
}



void CMixerMasterStrip::dsb_vol_l_valueChanged (double d)
{
 if (update_strip_controls)
     return;
   
  float f = 1.0f;
  
  if (d == 0.0)
     {
      p_track->volume_left = 1.0f;
      
      if (p_track->linked_channels)
         {
          p_track->volume_right = p_track->volume_left;
          update_strip_controls = true;
          dsb_vol_r->setValue (d);
          update_strip_controls = false;
         }
      
      return;
     }

   f = db2lin (d);

   p_track->volume_left = f;
   
   if (p_track->linked_channels)
      {
       p_track->volume_right = p_track->volume_left;
       update_strip_controls = true;
       dsb_vol_r->setValue (d);
       update_strip_controls = false;
      }
      
}


void CMixerMasterStrip::dsb_vol_r_valueChanged (double d)
{
 if (update_strip_controls)
     return;
   
  float f = 1.0f;
  
  if (d == 0.0)
     {
      p_track->volume_right = 1.0f;
      
      if (p_track->linked_channels)
         {
          p_track->volume_left = p_track->volume_right;
          update_strip_controls = true;
          dsb_vol_l->setValue (d);
          update_strip_controls = false;
         }
      
      return;
     }

   f = db2lin (d);

   p_track->volume_right = f;
   
   if (p_track->linked_channels)
      {
       p_track->volume_left = p_track->volume_right;
       update_strip_controls = true;
       dsb_vol_l->setValue (d);
       update_strip_controls = false;
      }
}


CMixerMasterStrip::CMixerMasterStrip (CMasterTrack *ptrk, QWidget *parent): QGroupBox (parent)
{
  p_track = ptrk;
  
 qDebug() << "CMixerMasterStrip::CMixerMsterStrip (CTrack *ptrk): " << p_track->track_name;
  
//  QString gbs = "CMixerMasterStrip { background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #E0E0E0, stop: 1 #FFFFFF);"
  QString gbs = "CMixerMasterStrip {"
    "border: 1px solid gray;"
    //"border-radius: 5px;"
    "margin-top: 1ex;} "
"CMixerStrip::title {"
    "subcontrol-origin: margin;"
    "subcontrol-position: top center;" 
    "padding: 0 3px;}";
    //"background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FFOECE, stop: 1 #FFFFFF);}";
  
  setStyleSheet (gbs);
 
  update_strip_controls = true;
  
  QVBoxLayout *v_main = new QVBoxLayout;
  setLayout (v_main);

  QGroupBox *gb_controls = new QGroupBox (tr ("Controls"));
  QVBoxLayout *v_controls = new QVBoxLayout;
  gb_controls->setLayout (v_controls);
  
    
  QPushButton *bt_properties = new QPushButton (tr ("Properties"));
  v_controls->addWidget (bt_properties);
  
  
  QGroupBox *gb_fx = new QGroupBox (tr ("FX"));
  QVBoxLayout *v_fx = new QVBoxLayout;
  gb_fx->setLayout (v_fx);
  
  QPushButton *bt_inserts = new QPushButton (tr ("Inserts"));
  v_fx->addWidget (bt_inserts);
  
  QPushButton *bt_sends = new QPushButton (tr ("Sends"));
  v_fx->addWidget (bt_sends);
  
 
  
  QGroupBox *gb_vol = new QGroupBox (tr ("Level"));
  QVBoxLayout *v_vol = new QVBoxLayout;
  gb_vol->setLayout (v_vol);

        QHBoxLayout *h_vol = new QHBoxLayout;
        QLabel *l_vol_l = new QLabel (tr ("L:"));
        QLabel *l_vol_r = new QLabel (tr ("R:"));
        
        cb_linked_channels = new QCheckBox (tr ("Link channels"));
        cb_linked_channels->setTristate (false);
        cb_linked_channels->setChecked (p_track->linked_channels);
        connect (cb_linked_channels, SIGNAL(toggled(bool)), this, SLOT(cb_linked_channels_toggled(bool)));
      
        dsb_vol_l = new QDoubleSpinBox;
        dsb_vol_l->setDecimals (2); 
        dsb_vol_l->setRange (-90, 6.0);
        dsb_vol_l->setSingleStep (0.01);
        dsb_vol_l->setValue (float2db (p_track->volume_left));
    
        connect (dsb_vol_l, SIGNAL(valueChanged(double)), this, SLOT(dsb_vol_l_valueChanged(double)));
 
        dsb_vol_r = new QDoubleSpinBox;
        dsb_vol_r->setDecimals (2); 
        dsb_vol_r->setRange (-90, 6.0);
        dsb_vol_r->setSingleStep (0.01);
        dsb_vol_r->setValue (float2db (p_track->volume_right));
    
        connect (dsb_vol_r, SIGNAL(valueChanged(double)), this, SLOT(dsb_vol_r_valueChanged(double)));
       
        h_vol->addWidget (l_vol_l);
        h_vol->addWidget (dsb_vol_l);
        
        h_vol->addWidget (l_vol_r);
        h_vol->addWidget (dsb_vol_r);
        
        v_vol->addLayout (h_vol);
        v_vol->addWidget (cb_linked_channels);
  

  QGroupBox *gb_meter = new QGroupBox (tr ("Meter"));
  QHBoxLayout *h_meter = new QHBoxLayout;
  gb_meter->setLayout (h_meter);
    
  level_meter = new CLevelMeter (32, 16, 128, "#ffaa00", "#ffaa7f");
  rms_meter = new CLevelMeter (32, 16, 128, "#aa557f", "#55557f");

  QVBoxLayout *v_level_meter = new QVBoxLayout;
  QLabel *l_level_meter = new QLabel (tr ("Peaks"));
  
  v_level_meter->addWidget (l_level_meter);
  v_level_meter->addWidget (level_meter);

  QVBoxLayout *v_rms_meter = new QVBoxLayout;
  QLabel *l_rms_meter = new QLabel (tr ("RMS"));
  
  v_rms_meter->addWidget (l_rms_meter);
  v_rms_meter->addWidget (rms_meter);


  h_meter->addLayout (v_level_meter);
  h_meter->addLayout (v_rms_meter);
   
  update_strip_controls = false;
  

  v_main->addWidget (gb_controls);
  
  v_main->addWidget (gb_fx);
  
  v_main->addWidget (gb_vol);
  
  v_main->addWidget (gb_meter);
  
}


void CWavTrack::call_properties_wnd()
{
//track name

//track commnents

  QDialog dialog;
  
  dialog.setWindowTitle (tr ("Track properties"));

  
  QVBoxLayout *v_box = new QVBoxLayout;
  dialog.setLayout (v_box);

  QHBoxLayout *h_trackname = new QHBoxLayout;
  v_box->addLayout (h_trackname);

  QLabel *l_trackname = new QLabel (tr ("Track name:"));
  QLineEdit *ed_trackname = new QLineEdit;
  ed_trackname->setText (track_name);
  
  h_trackname->addWidget (l_trackname);
  h_trackname->addWidget (ed_trackname);
  
  QGroupBox *gb_comments = new QGroupBox (tr ("Comments"));
  QVBoxLayout *v_comments = new QVBoxLayout;
  gb_comments->setLayout (v_comments);
  
  QPlainTextEdit *pte_comments = new QPlainTextEdit;
  v_comments->addWidget (pte_comments);
  pte_comments->setPlainText (track_comment);
  
  v_box->addWidget (gb_comments);
 
   
  QPushButton *bt_done = new QPushButton (tr ("Done"), &dialog);
  bt_done->setDefault (true);
  v_box->addWidget (bt_done);

  //QVBoxLayout *h_trackname = new QHBoxLayout;
  //v_box->addLayout (h_trackname);

  
   
  connect (bt_done, SIGNAL(clicked()), &dialog, SLOT(accept()));

   if (dialog.exec())
      {
       track_comment = pte_comments->toPlainText();
       track_name = ed_trackname->text();
      }   
      
  dialog.close();   

  update_strip();
  
  
  //и таблицу обновить!
  //p_project->update_table();
  
  gb_track->setTitle (track_name);
}




void CProject::tracks_window_create()
{
  wnd_tracks = new QWidget;
  wnd_tracks->setWindowTitle (tr ("Tracks"));

  QHBoxLayout *h_main = new QHBoxLayout;
  wnd_tracks->setLayout (h_main);
  
  QGroupBox *gb_tracks = new QGroupBox (tr ("Tracks"));
  QVBoxLayout *v_tracks = new QVBoxLayout;
  gb_tracks->setLayout (v_tracks);
 
  lw_tracks = new QListWidget;
  v_tracks->addWidget (lw_tracks);
 
 
  QHBoxLayout *h_tracks_buttons = new QHBoxLayout;
  v_tracks->addLayout (h_tracks_buttons);
  
  QPushButton *bt_tracks_new = new QPushButton ("New");
  QPushButton *bt_tracks_delete = new QPushButton("Delete");
  QPushButton *bt_tracks_up = new QPushButton ("Up");
  QPushButton *bt_tracks_down = new QPushButton ("Down");
  
    
  connect (bt_tracks_new, SIGNAL(clicked()), this, SLOT(bt_tracks_new()));  
  connect (bt_tracks_delete, SIGNAL(clicked()), this, SLOT(bt_tracks_delete()));  
  connect (bt_tracks_up, SIGNAL(clicked()), this, SLOT(bt_tracks_up()));  
  connect (bt_tracks_down, SIGNAL(clicked()), this, SLOT(bt_tracks_down()));  
    
    
  h_tracks_buttons->addWidget (bt_tracks_new);  
  h_tracks_buttons->addWidget (bt_tracks_delete);  
  h_tracks_buttons->addWidget (bt_tracks_up);  
  h_tracks_buttons->addWidget (bt_tracks_down);  
  
    
  h_main->addWidget (gb_tracks);
}


void CProject::bt_tracks_new()
{
  track_insert_new();
}


void CProject::bt_tracks_delete()
{
  int index = lw_tracks->currentRow();
  
  QListWidgetItem *item = lw_tracks->takeItem (index);
  delete item;

  //t->mixer_strip
  
  CTrack *track = tracks[index];
  
  tracks.removeAt (index);

  delete track;

  
  //обновить таблицу
  
  
  
  //обновить микшер

/*
 for (int i = 0; i < tracks.size(); i++)
     { 
      track_add_strip_to_mixer_window (tracks[i]);
     } 
*/

}

void CProject::bt_tracks_up()
{

}


void CProject::bt_tracks_down()
{

}



void CProject::tracks_window_show()
{
  lw_tracks_refresh();

  if (! wnd_tracks->isVisible())
     wnd_tracks->show();
  else
     wnd_tracks->close();
}


void CProject::lw_tracks_refresh()
{
  lw_tracks->clear();
  for (int i = 0; i < tracks.size(); i++)
     { 
      lw_tracks->addItem (tracks[i]->track_name);
     } 
}

#define REC_BUFFER_MULTILPLIER 48

void CWavTrack::record_start()
{
  int sndfile_format = 0;
  sndfile_format |= SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  SF_INFO sf;

  sf.samplerate = p_project->settings.samplerate;
  sf.channels = channels;
  sf.format = sndfile_format;
   
  if (! sf_format_check (&sf))
     {
      qDebug() << "! sf_format_check (&sf)";
      //recording = false;
      return;  
     }    
  
  //allocate recbuf
  if (fb_recbuffer)
     delete fb_recbuffer;
     
  fb_recbuffer = new CFloatBuffer (buffer_size_frames * REC_BUFFER_MULTILPLIER, 2);   
  
  fb_recbuffer->allocate_interleaved();

  QDateTime dt = QDateTime::currentDateTime();
  QString sdt = dt.toString ("yyyy-MM-dd@hh:mm:ss:zzz");
  sdt = sdt.replace ("@", "-");
  sdt = sdt.replace (":", "-");
    
  fname_rec_fullpath = p_project->paths.wav_dir + "/" + track_name + "-" + sdt + ".wav";
  hrecfile = sf_open (fname_rec_fullpath.toUtf8().data(), SFM_WRITE, &sf);
}

 
void CWavTrack::record_iteration (const void *input, size_t frameCount)
{
  float **pinput = (float **)input;

  if (fb_recbuffer->offset >= fb_recbuffer->length_frames) //SAVE BUFFER TO DISK
     {
      fb_recbuffer->offset = 0;
      
      if (channels == 1)
          sf_writef_float (hrecfile, fb_recbuffer->buffer[0], REC_BUFFER_MULTILPLIER * buffer_size_frames);
      else          
          {
           fb_recbuffer->fill_interleaved();
           sf_writef_float (hrecfile, (float *)fb_recbuffer->buffer_interleaved, REC_BUFFER_MULTILPLIER * buffer_size_frames);
          }
       
      fb_recbuffer->settozero();
     }

 //COPY TO BUFFER

  if (channels == 2)
     {
      memcpy (fb_recbuffer->buffer[0] + fb_recbuffer->offset, pinput[0], frameCount * sizeof (float));
      memcpy (fb_recbuffer->buffer[1] + fb_recbuffer->offset, pinput[1], frameCount * sizeof (float));
     }
  else  
  if (channels == 1)
     {
      if (mono_recording_mode == 0)
          memcpy (fb_recbuffer->buffer[0] + fb_recbuffer->offset, pinput[0], frameCount * sizeof (float));
      else   
          memcpy (fb_recbuffer->buffer[0] + fb_recbuffer->offset, pinput[1], frameCount * sizeof (float));
     }
  
  fb_recbuffer->offset += frameCount;
  //qDebug() << "fb_recbuffer->offset:" << fb_recbuffer->offset;
}


void CWavTrack::record_stop()
{
  if (channels == 1)
     {
      if (mono_recording_mode == 0)
         sf_writef_float (hrecfile, fb_recbuffer->buffer[0], fb_recbuffer->offset);
     }
  else
      {
       fb_recbuffer->fill_interleaved();
       sf_writef_float (hrecfile, (float *)fb_recbuffer->buffer_interleaved, fb_recbuffer->offset);
      }
 

  sf_close (hrecfile);
                          
  QFileInfo fi (fname_rec_fullpath);
  QString fname = fi.fileName();
               
  p_project->files.load_file (p_project->paths.wav_dir, fname);        
               
  CWaveClip *clip = new CWaveClip (this);
  clip->filename = fname;
  clip->file = p_project->files.wavs[clip->filename];
               
  clip->position_frames = record_insertion_pos_frames;
  clip->offset_frames = 0;
  clip->length_frames = clip->file->fb->length_frames;
               
  clip->name = fname;
  clips.append (clip);
                              
  CTrackTableWidget *w = (CTrackTableWidget*)table_widget;
  w->update_track_table (true);
}


CWavTrack::~CWavTrack()
{
  if (fb_recbuffer)
     delete fb_recbuffer;
}
  

ATrackWidget::ATrackWidget (CTrack *track, QWidget *parent): QWidget (parent)
{
  p_track = track;
  p_track->p_project->w_timeline->vbl_tracks->addWidget (this);
  
  setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
}


CTimeLine::CTimeLine (CProject *p, QWidget *parent): QWidget (parent)
{
  p_project = p;
  
  zoom_factor = 1;
  
  QVBoxLayout *vbl_main = new QVBoxLayout;
  setLayout (vbl_main);
  
  w_tracks = new QWidget;
  vbl_tracks = new QVBoxLayout;
  w_tracks->setLayout (vbl_tracks);
  
  QScrollArea *scra_tracks = new QScrollArea;
  scra_tracks->setWidgetResizable (true);
  scra_tracks->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
  
  scra_tracks->setWidget (w_tracks);

  sb_timeline = new QScrollBar (Qt::Horizontal);
  
  connect (sb_timeline, SIGNAL(valueChanged(int )), this, SLOT(sb_timeline_valueChanged (int )));
 
  

  vbl_main->addWidget (scra_tracks);
 
 
  vbl_main->addWidget (sb_timeline);
 
  update_sb_timeline_zoom();
 
  //p_project->frames_per_pixel = p_project->song_length_frames / w_tracks->width();
  
//  qDebug() << "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{";
  
//  qDebug() << "!!!!!!!!!! p_project->frames_per_pixel: " << p_project->frames_per_pixel;

//  qDebug() << "{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{";
  
}


CWAVTrackWidget::CWAVTrackWidget (CTrack *track, QWidget *parent): ATrackWidget (track, parent)
{
  qDebug() << "CWAVTrackWidget::CWAVTrackWidget";

}


void CWAVTrackWidget::paintEvent (QPaintEvent *event)
{
// qDebug() << "CWAVTrackWidget::paintEvent";
//  QWidget::paintEvent (event);
  
  if (! p_track) 
     return;
  
  if (! p_track->p_project->w_timeline)
     return;
  
  int window_start = p_track->p_project->w_timeline->sb_timeline->value();
  int window_end = window_start + p_track->p_project->w_timeline->w_tracks->width();
  
  //scaled = window_start * p_track->p_project->w_timeline->frames_per_pixel();
  
  
  QPainter painter (this);
  
  prepare_image();  
  painter.drawImage (0, 0, image);
  
  
  if (p_track->focused)
      painter.setPen (Qt::red);
  else    
      painter.setPen (Qt::black);
  
  painter.drawRect (rect());
  painter.drawText (rect(), p_track->track_name);
    
    
  event->accept();
}


QSize ATrackWidget::sizeHint() const
{
  return QSize (parentWidget()->width(), 100);

}



void CWAVTrackWidget::mousePressEvent (QMouseEvent *event)  
{
  
  setFocus (Qt::OtherFocusReason);
  //mouse_pressed = true;
      
  for (int i = 0; i < p_track->p_project->tracks.size(); i++)
       p_track->p_project->tracks[i]->focused = false;
         
      
  p_track->focused = true;    
  
  
  bool ctrl_pressed = (event->modifiers() & Qt::ControlModifier);  
  
  if (event->button() == Qt::LeftButton)
     {
  //найти клип под курсором, если найден - выделить, не найден - установить курсор 
  
      size_t frame_at_pos = p_track->p_project->w_timeline->sb_timeline->value() + event->x() + TRACKCONTROLW;
      frame_at_pos *= p_track->p_project->w_timeline->frames_per_pixel();
      
      //bool add_to_sel = 
      
      p_track->p_project->w_timeline->clip_select_at_pos (frame_at_pos, ctrl_pressed);
  
  /*
      qDebug() << "frame_at_pos " << frame_at_pos;
  
      for (int i = 0; i < p_track->p_project->tracks.size(); i++)
          {
           CTrack *t = p_track->p_project->tracks[i];
           for (int j = 0; j < t->clips.size(); j++)
               {
                CClip *clip = t->clips[j];
                if (frame_at_pos > clip->position_frames && frame_at_pos < 
                    clip->position_frames + clip->length_frames)
                    {
                     qDebug() << clip->name;
                    
                    } 
               
               } 
          } 
           
  
  
      p_track->p_project->w_timeline->w_tracks->update();
      
      */
     }
  
  event->accept();
  
}



void ATrackWidget::wheelEvent (QWheelEvent *event)
{
  scale (event->delta());
  event->accept();
}  


void ATrackWidget::resizeEvent (QResizeEvent *event)
{
  //recalc_view();
  
 // section_from = scrollbar->value();
  //section_to = width() + scrollbar->value();
  
  prepare_image();
}


void CWAVTrackWidget::keyPressEvent (QKeyEvent *event)
{
  //if (event->key() == Qt::Key_Delete)
  
  
  event->accept();
}  


void CWAVTrackWidget::prepare_image()
{

  QImage img (width() - TRACKCONTROLW, height(), QImage::Format_RGB32);
  QPainter painter (&img);
  
  
  img.fill (QColor ("white")); 
  
  //painter.setPen (cl_waveform_foreground);
    
  //img.fill (cl_waveform_background.rgb()); 
  
  
  if (! p_track) 
     return;
  
  if (! p_track->p_project->w_timeline)
     return;
  
  int scale = p_track->p_project->w_timeline->frames_per_pixel();
  
  int window_start = p_track->p_project->w_timeline->sb_timeline->value();
  int window_end = window_start + p_track->p_project->w_timeline->w_tracks->width() - TRACKCONTROLW;
  
  int window_length_frames = (p_track->p_project->w_timeline->w_tracks->width() - TRACKCONTROLW) * scale; 
  
  //scaled = window_start * p_track->p_project->w_timeline->frames_per_pixel();
  
  int start_pos_frames = window_start * scale;
  int end_pos_frames = window_end * scale;
  
    //size_t end_pos_frames = start_pos_frames + window_length_frames; //конец окна на дорожке

  int clips_count = p_track->clips.size();
 
  //fbtrack->settozero(); 

/*

сюда же вставить проверку, находятся ли первый и последний клипы в пределах курсора.
если нет, то вообще не рендерим больше дорожку

*/
     
  for (int i = 0; i < clips_count; i++)
      {
       CWaveClip *clip = (CWaveClip*)p_track->clips[i];

       if (clip->muted)
          continue;
      
       //qDebug() << "***** clip: " << clip->name << " **** " << clip->length_frames;
       
      // qDebug() << "clip: " << clip->name;
       //qDebug() << "clip len: " << clip->length_frames;
       
       //check is clip in range?
       
       size_t clip_start = clip->position_frames; //начало клипа на дорожке, положение
       size_t clip_end = clip_start + clip->length_frames;//конец клипа на дорожке, положение
       
       int clip_in_window = 0; //0 - not, 1 - window len <= clip, 2 - window len => clip
       
       if (window_length_frames >= clip->length_frames)
          {
           if ((start_pos_frames <= clip_start && clip_start <= end_pos_frames) ||
               (start_pos_frames <= clip_end && clip_end <= end_pos_frames)) 
              clip_in_window = 2; 
          }
       else
           {
            if ((clip_start <= start_pos_frames && start_pos_frames <= clip_end) ||
                (clip_start <= end_pos_frames && end_pos_frames <= clip_end))
               clip_in_window = 1;    
            }  
       
     //  qDebug() << clip_in_window;
      
       if (clip_in_window == 0)
          continue;
                
//       qDebug() << "clip: " << clip->name << " is in window";  
        
       //теперь вычислить отрезок клипа, входящий в окно
       // именно смещение клипа относительно начала клипа и смещение относительно конца клипа
        
       size_t clip_data_start = 0;  //где в клипе начало попадающего в окно отрезка? относительно начала клипа
       size_t clip_insertion_pos = 0; //куда вставлять в буфер дорожки?
       size_t clip_data_length = 0;
        
       if (clip_in_window == 1)
          {
         //  qDebug() << clip->name << " clip_in_window == 1";
           
           if (start_pos_frames <= clip_start)
             {
           //   qDebug() << "x1";
              clip_insertion_pos = clip_start - start_pos_frames;
              clip_data_start = 0;
              clip_data_length = end_pos_frames - clip_start;
             }
           else   
               if (start_pos_frames > clip_start)
                  {
                   //qDebug() << "x2";

                   clip_insertion_pos = 0; //по границе начала окна
                   clip_data_start = start_pos_frames - clip_start;
                   
                   //qDebug() << "clip_data_start: " << clip_data_start;
                   //qDebug() << "clip_end: " << clip_end;
                   //qDebug() << "end_pos_frames: " << end_pos_frames;
            
                   if (clip_end >= end_pos_frames)
                       clip_data_length = window_length_frames;
                   else
                      {
                      //qDebug() << "x3";   
                      // clip_data_length = end_pos_frames - clip_start;
                        clip_data_length = clip_end - start_pos_frames;
                      
                       
                      } 
                       
                   qDebug() << "!!! clip_data_length: " << clip_data_length;
                
                 }
           }
        
        if (clip_in_window == 2)
           {
    //       qDebug() << "clip_in_window == 2";
          
            if (start_pos_frames <= clip_start)
               {
                clip_insertion_pos = clip_start - start_pos_frames;
                clip_data_start = 0;
                
                if (clip_start >= start_pos_frames && clip_end <= end_pos_frames)
                   clip_data_length = clip->length_frames;
                else   
                   clip_data_length = end_pos_frames - clip_start;
               }
            else   
                if (start_pos_frames > clip_start)
                   {
                    clip_insertion_pos = 0; //по границе начала окна
                    clip_data_start = start_pos_frames - clip_start;
          
                    if (clip_start >= start_pos_frames && clip_end >= end_pos_frames)
                        clip_data_length = clip->length_frames;
                    else   
                        clip_data_length = clip_end - start_pos_frames;
                   }
            }
          
        //ошибка с clip_data_length, может быть больше, чем действительно есть?  
          
      //  qDebug() << "clip_data_start: " << clip_data_start;
//        qDebug() << "clip_data_length: " << clip_data_length;
  //      qDebug() << "clip_insertion_pos: " << clip_insertion_pos;
        
      
        size_t extoffs = (clip->offset_frames + clip_data_start) * clip->playback_rate;
       
           //ошибка в вычислении clip_insertion_pos???     
           //тут может быть вылет!!!  
        
        if (clip->playback_rate == 1.0f && clip->file)
          {
           
           
           //образец: clip->file->fb->copy_to_pos (fbtrack, extoffs, clip_data_length, clip_insertion_pos);
           
           int x = TRACKCONTROLW + clip_insertion_pos / scale - window_start;
           int y = 0;
           int w = clip_data_length / scale;
           int h = height();
           
           QRect r (x, y, w, h);
           
           QBrush clip_brush;
           
           if (! clip->selected)
              clip_brush.setColor ("blue");
           else    
               clip_brush.setColor ("green");
               
           clip_brush.setStyle  (Qt::Dense3Pattern);
           
           QFont f (painter.font());
           f.setPointSize (12);    

           painter.setFont (f);
    
//           painter.font().setPointSize (12);    
           painter.fillRect (r, clip_brush);
           painter.drawText (r, clip->name);
           
          } 
       }   
       
  image = img; 
}



void ATrackWidget::recalc_view()
{
  //qDebug() << "CWaveform::recalc_view() - start";
   /*
  sections_total = width() * scale_factor;
  
  if (sections_total == 0)
     return;

  frames_per_section = ceil (fb->length_frames / sections_total);
  
  if (frames_per_section < FRAMES_PER_SECT_MAX)
     frames_per_section = FRAMES_PER_SECT_MAX;
  
  scrollbar->setMinimum (0);
  scrollbar->setMaximum (sections_total - width());
*/
  // << "CWaveform::recalc_view() - end";
}


void ATrackWidget::scale (int delta)
{
 //qDebug() << "CWaveform::scale - start";

  qDebug() << "delta " << delta;

  if (delta > 0)
     p_track->p_project->w_timeline->zoom_factor++; //  p_track->p_project->w_timeline->zoom_factor
  if (delta < 0)
     p_track->p_project->w_timeline->zoom_factor--;
     
  if (p_track->p_project->w_timeline->zoom_factor < 1)   
     p_track->p_project->w_timeline->zoom_factor = 1;
     

  p_track->p_project->w_timeline->update();
  
  p_track->p_project->w_timeline->w_tracks->update();
  
  p_track->p_project->w_timeline->update_sb_timeline_zoom();


  qDebug() << "zoom_factor: " << p_track->p_project->w_timeline->zoom_factor;

/*
  if (frames_per_section == 0)
     return;

  int old_section_from = get_section_from();
  int old_frame_from = old_section_from * frames_per_section;

  if (delta > 0)
     scale_factor += 0.1f;
  else    
     scale_factor -= 0.1f;
    
  if (scale_factor < 1.0f)
     scale_factor = 1.0f;

  if ((width() * scale_factor) >= fb->length_frames - 1) //can be scale factor so large?
      return;

  recalc_view();
  prepare_image();
  
  int new_section = old_frame_from / frames_per_section;   
  scrollbar->setValue (new_section);

  update();   
  timeruler->update();
*/
  //qDebug() << "CWaveform::scale - end";
}


size_t CTimeLine::frames_per_pixel()
{
  int t = p_project->song_length_frames / w_tracks->width() / zoom_factor;
  if (t == 0)
     t = 1;
     
  return t;   
}


void CTimeLine::sb_timeline_valueChanged (int value)
{
  w_tracks->update();
  qDebug() << "CTimeLine::sb_timeline_valueChanged " << value;

}

void CTimeLine::update_sb_timeline_zoom()
{
  int val_new = sb_timeline->value() / frames_per_pixel();
  int max_new = p_project->song_length_frames / frames_per_pixel();  
  sb_timeline->setValue (val_new);
  sb_timeline->setMaximum (max_new);
}


int CTimeLine::clip_select_at_pos (size_t frame, bool add_to_selection)
{
  for (int i = 0; i < p_project->tracks.size(); i++)
      {
       CTrack *t = p_project->tracks[i];
       for (int j = 0; j < t->clips.size(); j++)
           {
            CClip *clip = t->clips[j];
            
            if (! add_to_selection)
                clip->selected = false;
            
            if (t->focused)
            if (frame > clip->position_frames && frame < 
               clip->position_frames + clip->length_frames)
                 {
                  if (! add_to_selection)
                    clip->selected = true;
                  else
                      clip->selected = ! clip->selected;
                  
                  qDebug() << clip->name;
                    
                 } 
               
            } 
          } 
           
  
  
  w_tracks->update();
   
  return 1;
}



CClip* CTimeLine::get_selected_clip()
{
  CClip *clip = 0;

  for (int i = 0; i < p_project->tracks.size(); i++)
      {
       CTrack *t = p_project->tracks[i];
       for (int j = 0; j < t->clips.size(); j++)
           {
            clip = t->clips[j];
            if (clip->selected)
               return clip;
           }
      }
      
  return clip;    
}


void CTimeLine::clip_selected_delete()
{
 CClip *clip = 0;

  for (int i = 0; i < p_project->tracks.size(); i++)
      {
       CTrack *t = p_project->tracks[i];
       for (int j = 0; j < t->clips.size(); j++)
           {
            clip = t->clips[j];
            
            if (clip->selected)
               {
                qDebug() << "delete " << clip->name;
                clip = t->clips.takeAt (j);
                delete clip;
               }
           }
      }
}
