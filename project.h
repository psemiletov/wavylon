#ifndef PROJECT_H
#define PROJECT_H


/*
  
 CFile (io)

 CFiles //pool
  
 CClip = clip (link to CFile)
 
 CTrack (clips list)

 CProject (paths, tracks) 
  
 * */


#include <QObject> 
#include <QString> 
#include <QList> 
#include <QLineEdit>
#include <QDialog>
#include <QTimeEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QScrollArea>
#include <QScrollBar>

#include <QAbstractItemDelegate>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QItemDelegate>
#include <QScrollArea>
#include <QDial>
#include <QTimer>

#include <sndfile.h>


#include "levelmeter.h"
#include "floatbuffer.h"
#include "fxrack.h"



class CProject;
class CTrack;
class CMasterTrack;

class ATrackWidget;
class CWAVTrackWidget;

class CMixerWindow;
class CMixerStrip;
class CMixerMasterStrip;

class CTimeLine;

class CMixerMasterStrip: public QGroupBox
{
Q_OBJECT
  
public:  

  CMasterTrack *p_track;
  
  bool update_strip_controls;

  CLevelMeter *level_meter;
  CLevelMeter *rms_meter;
  
  QDoubleSpinBox *dsb_vol_l;
  QDoubleSpinBox *dsb_vol_r;
  
  QCheckBox *cb_linked_channels;
 
  
  CMixerMasterStrip (CMasterTrack *ptrk, QWidget *parent = 0);


public slots:

 
  void dsb_vol_l_valueChanged (double d);
  void dsb_vol_r_valueChanged (double d);
  
  void cb_linked_channels_toggled (bool checked);
};


class CMixerStrip: public QGroupBox
{
Q_OBJECT
  
public:  

  CTrack *p_track;
  
  bool update_strip_controls;
  
  CLevelMeter *level_meter;
  CLevelMeter *rms_meter;
  
  QDoubleSpinBox *dsb_pan;
  QDoubleSpinBox *dsb_vol;
  
  QCheckBox *cb_mute;
  QCheckBox *cb_solo;
  
  QCheckBox *cb_arm;
  QCheckBox *cb_monitor_input;
  
  
  CMixerStrip (CTrack *ptrk, QWidget *parent = 0);


public slots:

  void dsb_pan_valueChanged (double d);
  void dsb_vol_valueChanged (double d);
 
  
  void cb_monitor_input_toggled (bool checked);
  
  void cb_mute_toggled (bool checked);
  void cb_solo_toggled (bool checked);
  void cb_arm_toggled (bool checked);

  void call_track_properties();
  void call_track_inserts();

};


class CWAVPlayer: public QObject
{
  Q_OBJECT

public:

  CFloatBuffer *p_buffer; //pointer to external source
  
  size_t offset_frames; //current offset
  
  int state; // 0 - stop, 1 - play, 2 - pause
  
  CWAVPlayer();
  ~CWAVPlayer();
  
  void link (CFloatBuffer *a_p_buffer);
  
  void play();
};


 
class CProjectPaths: public QObject
{
  Q_OBJECT

public:

  QString xml_dir;
  QString wav_dir;
  QString output_dir; //for renders

  QString project_dir;  
  QString fname_project;
};



class CSoundSource: public QObject
{
  Q_OBJECT

public:

  QString id; //can be filename for example;

};


class CWAVSource: public CSoundSource
{
  Q_OBJECT
  
public:
  
  CFloatBuffer *fb;
  void load (const QString &path);

};


class CWavFiles: public QObject
{
  Q_OBJECT

public:

  QHash <QString, CWAVSource*> wavs;
  void load_all_from_dir (const QString &path);
  void load_file (const QString &path, const QString &f);

  ~CWavFiles();
};


class CClip: public QObject
{
  Q_OBJECT

public:

  CTrack *p_track;

  QString type;
  QString name;
    
  bool muted;
  bool selected;
  
  float playback_rate;

  size_t offset_frames; //offset from the start of file, in frames 
  size_t length_frames; //length of the clip. the right border = offset_frames + length_frames 

  size_t position_frames; //position at the track, in frames

  CClip (CTrack *ptr);

  virtual void call_ui() = 0;
  virtual void repeat_cloned (int n) = 0; //repeat clip N times after this
  virtual void repeat_unique (int n) = 0; //repeat clip N times after this

};


/*
 
  
  CClip is the parent of
  
  CWaveClip
  CMIDIClip
  CTrackerClip
  CDrumMachineClip
  CSinusSynthClip 
  etc
  
  
 */


class CWaveClip: public CClip
{
  Q_OBJECT

public:

  CWAVSource *file;
  
  QString filename;
  
  CWaveClip (CTrack *ptr);
  
  void call_ui();
  void repeat_cloned (int n); //repeat clip N times after this
  void repeat_unique (int n); //repeat clip N times after this

}; 


class CTrack: public QObject
{
  Q_OBJECT

public:

  CProject *p_project;
  
  bool focused;
  
  ATrackWidget *track_widget;
  
  CMixerStrip *mixer_strip;
  QGroupBox *gb_track;
 
  size_t channels;
  
  float pan;  //0 - left, 0.5 - middle, 1 - right
  float volume;

  size_t record_insertion_pos_frames;
  
  bool mute;
  bool solo;
  bool arm;
  
  bool monitor_input;
  
  QString track_type;
  QString track_name;
  QString track_comment;
 
  QList <CClip*> clips; 

  void *table_widget;

  CFxRack fxrack;

  CFloatBuffer *fbtrack; //clips will be rendered here
  
  //renders clips to buffer from pos 
  virtual size_t render_portion (size_t start_pos_, size_t window_length_frames) = 0;  

  virtual void record_start() = 0;
  virtual void record_iteration (const void *input, size_t frameCount) = 0;
  virtual void record_stop() = 0;

  virtual void call_properties_wnd() = 0;
  void update_strip();

  CTrack (CProject *prj, int nchannels);
  virtual ~CTrack();
};



class QTimeEditItemDelegate: public QItemDelegate
{
  Q_OBJECT

public:

  QTimeEditItemDelegate (QObject *parent = 0): QItemDelegate (parent) {};
  ~QTimeEditItemDelegate () {};

  QWidget* createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index) const;
  void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const ;

};


class CTrackTableWidget: public QTableWidget
{
  Q_OBJECT

public:

  CTrack *p_track;

  void update_track_table (bool sort = true);
  
  void keyPressEvent (QKeyEvent *event);
};


class CWavTrack: public CTrack
{
  Q_OBJECT

public:

  QString fname_rec_fullpath;
  SNDFILE *hrecfile;
  
  CFloatBuffer *fb_recbuffer;
  
  CWavTrack (CProject *prj, int nchannels);
  ~CWavTrack();
  
  size_t render_portion (size_t start_pos_frames, size_t window_length_frames);  
  
  void record_start();
  void record_iteration (const void *input, size_t frameCount);
  void record_stop();

  
  void call_properties_wnd();
};


class CMasterTrack: public QObject
{
  Q_OBJECT

public:
  
  CProject *p_project;
  
  CMixerMasterStrip *mixer_strip;
   
  float volume_left;
  float volume_right;
  bool linked_channels;
  
  QString track_name;
  QString track_comment;
 
  size_t channels;
  
  CFloatBuffer *fb;

  CMasterTrack (CProject *prj);
  ~CMasterTrack();

  void update_strip();
};



class CProjectSettings: public QObject
{
  Q_OBJECT
  
public:  

  int samplerate;
  int bpm;
  int panner;

  QDialog *settings_window;
  QLineEdit *ed_samplerate;
  QLineEdit *ed_bpm;
  
  QComboBox *cmb_panner;

  
  CProjectSettings();
  
  void call_ui (bool creation = false);


public slots:

 void ui_done();
};


class CProject: public QObject
{
  Q_OBJECT
  
public:  
  
  bool update_pos_slider;
 
  bool recording;
  
  //int frames_per_pixel;
  
  size_t cursor_frames;
  
  size_t mixbuf_window_start_frames_one;
  size_t mixbuf_window_length_frames_one;

  CFloatBuffer *temp_mixbuf_fb; 

  size_t song_length_frames;


  size_t tracks_window_start_frames;
  size_t tracks_window_length_frames;
  size_t tracks_window_inner_offset; //offset of mixbuf at tracks window

  CMasterTrack *master_track;
  
  CProjectPaths paths;
  CProjectSettings settings;

  CWavFiles files;

  QString comments;

  //CWAVPlayer *test_player;

  QWidget *wnd_files;
  QListWidget *lw_wavclips;
  QListWidget *lw_wavs;
  
  QListWidget *lw_tracks;
  
  QTabWidget *tab_project_ui;
  
  QWidget *wnd_tracks;
  
  
  CTimeLine *w_timeline;
  
  QWidget *table_container; //container for tables
  QScrollArea *table_scroll_area;
  QHBoxLayout *table_widget_layout;
  
  // QGraphicsView *table_widget;
   //QGraphicsScene *table_scene;
   
  QWidget *transport_simple; 
  QSlider *slider_position;
 
  QTimeEdit *ted_position;

  CMixerWindow *mixer_window;

  QTimer gui_updater_timer; 
  
  QList <CTrack *> tracks;
  
  CProject();
  ~CProject();
  
  void project_new (const QString &fname);
  bool project_open (const QString &fname);

  bool wav_copy (const QString &fname); //copy or copy converted
  void lw_wavs_refresh();


  void save_project();
  void load_project();

  //void mixbuf_one(); 
  
  size_t tracks_render_next();
  int mixbuf_render_next (int rendering_mode = 0, const void *inpbuf = 0);

  void mixbuf_record();
  void mixbuf_play();
  void mixbuf_stop();
  

  void files_window_create();

  void tracks_window_create();

  void files_window_show();
  void tracks_window_show();

  void lw_tracks_refresh();


  void track_add_strip_to_mixer_window (CTrack *t);

  void create_widgets();
  
  void list_tracks();
  
  void update_table (bool sort = true);

  void refresh_song_length();
  
  void mixdown_to_default();
  
  void table_track_create (CTrack *t);
  void table_clip_props();
  
  void track_insert_new();
  void track_delete (int track_number);
  void track_swap (int track_number1, int track_number2);
  
  
public slots:

  void bt_tracks_new();
  void bt_tracks_delete();
  void bt_tracks_up();
  void bt_tracks_down();

  void bt_wavs_add_click();
  void bt_wavs_play_click();
  void bt_wavs_edit_click();
  void bt_wavs_clip_click();
  void bt_wavs_del_click();


  void bt_transport_play_click();
  void bt_transport_stop_click();
  void bt_transport_rewind_click();
  void bt_transport_forward_click();
  void bt_transport_record_click();


  void bt_wavclip_add_click();
  void bt_wavclip_play_click();
  void bt_wavclip_edit_click();
  void bt_wavclip_del_click();
  
  
  void table_cellSelected (int row, int col);
  void table_itemChanged (QTableWidgetItem *item);
  void table_itemActivated (QTableWidgetItem * item);
  
  void slider_position_valueChanged (int value);
  

  void gui_updater_timer_timeout();  
};


class CProjectManager: public QObject
{
Q_OBJECT
  
public:  

  CProject *project;
  QTabWidget *tab_project_ui;
  
  CProjectManager();
  ~CProjectManager();
  
  void create_new_project (const QString &fname);
  bool project_open (const QString &fname);
  void project_save();
};


class CMixerWindow: public QWidget
{
Q_OBJECT
  
public:  

  CProject *p_project;
  
  QHBoxLayout *h_main;
  QHBoxLayout *h_channel_strips_container;

  QScrollArea *channel_strips_scroll;
  QWidget *channel_strips_container;

  CMixerWindow (CProject *ptr);

};


//отображает одну дорожку
class ATrackWidget: public QWidget
{
Q_OBJECT
  
public:  

  QImage image;

  CTrack *p_track;
  
  //bool focused;
  
  ATrackWidget (CTrack *track, QWidget *parent = 0);

  QSize sizeHint() const;
  
  virtual void prepare_image() = 0;

  void scale (int delta);
  void recalc_view();


  void wheelEvent (QWheelEvent *event);
  void resizeEvent (QResizeEvent *event);

};



class CWAVTrackWidget: public ATrackWidget
{
Q_OBJECT
  
public:  


  CWAVTrackWidget (CTrack *track, QWidget *parent = 0);

  void prepare_image();

  void paintEvent (QPaintEvent *event);
  
  void mousePressEvent (QMouseEvent *event);  
  void keyPressEvent (QKeyEvent *event);
};


class CTimeLine: public QWidget
{
Q_OBJECT
  
public:  

  CProject *p_project;

 // QList <ATrackWidget*> track_widgets;

  QScrollArea *scra_tracks;
  QWidget *w_tracks;
  QVBoxLayout *vbl_tracks;
  QScrollBar *sb_timeline; //range is scaled to frames_per_pixel

  size_t zoom_factor;

  CTimeLine (CProject *p, QWidget *parent = 0);


  size_t frames_per_pixel();

  CClip* get_selected_clip(); //return first selected clip
  
  void clip_selected_delete();
  
  
  void update_sb_timeline_zoom();
 
  int clip_select_at_pos (size_t frame, bool add_to_selection);
    
public slots:

  void sb_timeline_valueChanged(int value);
};


#endif

