/***************************************************************************
 *   2015 by Peter Semiletov                                          *
 *   tea@list.ru                                                           *

started at 25 July 2010

Public Domain

 ***************************************************************************/


#include <iostream>

#include <samplerate.h>
#include <sndfile.h>
#include <portaudio.h>
#include <limits>

#include <QDebug>
#include <QStatusBar>

#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QPushButton>
#include <QToolButton>
#include <QLibraryInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QToolBar>
#include <QStyleFactory>
#include <QClipboard>
#include <QPainter>
#include <QInputDialog>
#include <QMimeData>

#include <QProxyStyle>
#include <QProcess>


#include "wavylon.h"
#include "utils.h"
#include "gui_utils.h"

#include "logmemo.h"


#include "db.h"
#include "fxlist.h"

#include "tio.h"
#include "project.h"


#define FM_ENTRY_MODE_NONE 0
#define FM_ENTRY_MODE_OPEN 1
#define FM_ENTRY_MODE_SAVE 2


#define C_METER_CPS 32;

extern CFxList *avail_fx;


bool b_altmenu;
extern bool b_monitor_input;

class MyProxyStyle: public QProxyStyle
{
public:
     int styleHint (StyleHint hint, const QStyleOption *option = 0,
                   const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const 
                   {
         
                    if (hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
                       return 0;
         
                    if (! b_altmenu && hint == QStyle::SH_MenuBar_AltKeyNavigation)
                       return 0;
                  
                    return QProxyStyle::styleHint (hint, option, widget, returnData);
                    }
     
   MyProxyStyle (QStyle *style = 0);
     
};


QSettings *settings;

QHash <QString, QString> global_palette;
extern int meter_cps;
extern int meter_msecs_delay;

extern size_t buffer_size_frames;
extern size_t buffer_size_frames_multiplier;


extern int proxy_video_decoder;


extern CFileFormats *file_formats;

extern QString temp_mp3_fname;

extern bool play_r;
extern bool play_l;
extern bool bypass_mixer;

QString fname_tempfile;


SNDFILE *file_temp_handle;


extern int mono_recording_mode;
extern int rec_channels;

extern int pa_device_id_in;
extern int pa_device_id_out;



QStringList lpa_apis;
QStringList lpa_devices;

//PaStream *pa_stream;
//PaStream *pa_stream_in;



CLogMemo *glog;


extern int transport_state;
extern double ogg_q;


int simple_counter = 0;


QStringList get_sound_devices()
{
  QStringList l;
  PaDeviceIndex devindex = Pa_GetDeviceCount() - 1; 	
  
  for (PaDeviceIndex i = 0; i < devindex; i++)
      {   
       const PaDeviceInfo *di = Pa_GetDeviceInfo (i);
       
#if defined(Q_OS_WIN) || defined(Q_OS_OS2)
       
       l.append (str_from_locale (di->name));
       
#else

       l.append (di->name);
       
#endif
     }  
  
  return l;
}


void CWavylon::init_styles()
{
#if QT_VERSION >= 0x050000

  QString default_style = qApp->style()->objectName();

  if (default_style == "GTK+") //can be buggy
     default_style = "Fusion";

#else

  QString default_style = qApp->style()->objectName();

  if (default_style == "GTK+") //can be buggy
     default_style = "Cleanlooks";

#endif

  fname_stylesheet = settings->value ("fname_stylesheet", ":/themes/TEA").toString();
   
  MyProxyStyle *ps = new MyProxyStyle (QStyleFactory::create (settings->value ("ui_style", default_style).toString()));
  
  QApplication::setStyle (ps);

  update_stylesheet (fname_stylesheet);
}


void pa_init (CWavylon *pe)
{
  PaError err = Pa_Initialize();
  qDebug() << Pa_GetErrorText (err);
  //pa_stream = 0;
  //pa_stream_in = 0;
}


void pa_done()
{
 /* if (pa_stream)
     {
      Pa_AbortStream (pa_stream);
      Pa_CloseStream (pa_stream);
     } 

  if (pa_stream_in)
     {
      Pa_AbortStream (pa_stream_in);
      Pa_CloseStream (pa_stream_in);
     } 
*/
  Pa_Terminate(); 	
}


void CWavylon::create_paths()
{
  QDir dr;
  dir_config = dr.homePath();

#ifdef Q_OS_WIN

  dir_config.append ("/wavylon");

#else

  dir_config.append ("/.config/wavylon");

#endif

  dr.setPath (dir_config);
  if (! dr.exists())
     dr.mkpath (dir_config);

  fname_fif.append (dir_config).append ("/fif");
  fname_places_bookmarks.append (dir_config).append ("/places_bookmarks");
  fname_tempfile = "/wavylon-temp-777.wav";   //append (QDir::tempPath()).append ("/Wavylon-temp-777.wav");
  fname_tempparamfile.append (QDir::tempPath()).append ("/wavylonparam.tmp");

  dir_profiles.append (dir_config).append ("/profiles");
  
  dr.setPath (dir_profiles);
  if (! dr.exists())
     dr.mkpath (dir_profiles);

  dir_sessions.append (dir_config).append ("/sessions");

  dr.setPath (dir_sessions);
  if (! dr.exists())
     dr.mkpath (dir_sessions);

  dir_palettes.append (dir_config).append ("/palettes");

  dr.setPath (dir_palettes);
  if (! dr.exists())
     dr.mkpath (dir_palettes);
     
     
  dir_themes.append (dir_config).append ("/themes");
  dr.setPath (dir_themes);
  if (! dr.exists())
     dr.mkpath (dir_themes);
   
}


void CWavylon::readSettings()
{
  b_altmenu = settings->value ("b_altmenu", "0").toBool(); 

  b_monitor_input = settings->value ("b_monitor_input", 0).toBool();

  proxy_video_decoder = settings->value ("proxy_video_decoder", "0").toInt(); 

  fname_def_palette = settings->value ("fname_def_palette", ":/palettes/EKO").toString();
  
  ogg_q = settings->value ("ogg_q", 0.5d).toDouble();

  QPoint pos = settings->value ("pos", QPoint (1, 1)).toPoint();
  QSize size = settings->value ("size", QSize (800, 600)).toSize();
  mainSplitter->restoreState (settings->value ("splitterSizes").toByteArray());
  resize (size);
  move (pos);

  //resampling_realtime_quality = settings->value ("resampling_realtime_quality", SRC_LINEAR).toInt();
  
  buffer_size_frames = settings->value ("buffer_size_frames", 2048).toInt();
  buffer_size_frames_multiplier = settings->value ("buffer_size_frames_multiplier", 100).toInt();
  
  mono_recording_mode = settings->value ("mono_recording_mode", 0).toInt();
}


void CWavylon::writeSettings()
{
  settings->setValue ("pos", pos());
  settings->setValue ("size", size());
  settings->setValue ("splitterSizes", mainSplitter->saveState());
  settings->setValue ("spl_fman", spl_fman->saveState());
  settings->setValue ("icons_size", iconSize().width());
  settings->setValue ("dir_last", dir_last);
  settings->setValue ("fname_def_palette", fname_def_palette);
  settings->setValue ("VERSION_NUMBER", QString (VERSION_NUMBER));
  settings->setValue ("state", saveState());

  if (project_manager->project)
     settings->setValue ("last_session", project_manager->project->paths.fname_project);
     
/*
  if (wnd_fxrack->isVisible())
     {
      settings->setValue ("wnd_fxrack.pos", wnd_fxrack->pos());
      settings->setValue ("wnd_fxrack.size", wnd_fxrack->size());
     }  
*/
  delete settings;
}


void CWavylon::create_main_widget()
{
qDebug() << " CWavylon::create_main_widget()";

  QWidget *main_widget = new QWidget;
  QVBoxLayout *v_box = new QVBoxLayout;
  main_widget->setLayout (v_box);

  main_tab_widget = new QTabWidget;
  
  main_tab_widget->setTabPosition (QTabWidget::East);
 
  main_tab_widget->setTabShape (QTabWidget::Triangular);

  tab_widget = new QTabWidget;
  
#if QT_VERSION >= 0x040500
  tab_widget->setMovable (true);
#endif
  
  //QPushButton *bt_close = new QPushButton ("X", this);
  //connect (bt_close, SIGNAL(clicked()), this, SLOT(close_current()));
  //tab_widget->setCornerWidget (bt_close);
  
  log_memo = new QPlainTextEdit;
  log = new CLogMemo (log_memo);
  
  glog = log;
  
  mainSplitter = new QSplitter (Qt::Vertical);
  v_box->addWidget (mainSplitter);

  cmb_fif = new QComboBox;
  cmb_fif->setInsertPolicy (QComboBox::InsertAtTop); 

  cmb_fif->setEditable (true);
  fif = cmb_fif->lineEdit();
  connect (fif, SIGNAL(returnPressed()), this, SLOT(search_find()));

  main_tab_widget->setMinimumHeight (10);
  log_memo->setMinimumHeight (10);

  mainSplitter->addWidget (main_tab_widget);
  mainSplitter->addWidget (log_memo);

  QHBoxLayout *lt_fte = new QHBoxLayout;
  v_box->addLayout (lt_fte);

  QToolButton *bt_find = new QToolButton (this);
  QToolButton *bt_prev = new QToolButton (this);
  QToolButton *bt_next = new QToolButton (this);

  bt_next->setArrowType (Qt::RightArrow);
  bt_prev->setArrowType (Qt::LeftArrow);

  connect (bt_find, SIGNAL(clicked()), this, SLOT(search_find()));
  connect (bt_next, SIGNAL(clicked()), this, SLOT(search_find_next()));
  connect (bt_prev, SIGNAL(clicked()), this, SLOT(search_find_prev()));

  bt_find->setIcon (QIcon (":/icons/search_find.png"));

  lt_fte->addWidget (cmb_fif);
  lt_fte->addWidget (bt_find);
  lt_fte->addWidget (bt_prev);
  lt_fte->addWidget (bt_next);

  mainSplitter->setStretchFactor (1, 1);

  
  idx_tab_edit = main_tab_widget->addTab (tab_widget, tr ("edit"));
 
  setCentralWidget (main_widget);
  
  project_manager->tab_project_ui = tab_widget;
 // connect (tab_widget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));
 
 qDebug() << " CWavylon::create_main_widget() - end";

}


CWavylon::CWavylon()
{

  init_db();
  
  avail_fx = new CFxList;
  
  pa_init (this);

 // play_r = true;
  //play_l = true;
  
  b_monitor_input = false;
  
  //bypass_mixer = false;    
  
  temp_mp3_fname = QDir::tempPath() + "/wavylon-temp-from-mp3.wav";
     
  file_temp_handle = 0;
  

//  meter_cps = C_METER_CPS;
//  meter_msecs_delay = 1000 / meter_cps;

//  qDebug() << "meter_msecs_delay = " << meter_msecs_delay;

  ui_update = true;

  file_formats_init();

  lv_menuitems = NULL;
  fm_entry_mode = FM_ENTRY_MODE_NONE;
  
  idx_tab_edit = 0;
  idx_tab_tune = 0;
  idx_tab_fman = 0;
  idx_tab_learn = 0;
  
  create_paths();
  
  QString sfilename = dir_config + "/wavylon.conf";
  settings = new QSettings (sfilename, QSettings::IniFormat);

  if (settings->value ("override_locale", 0).toBool())
     {
      QString ts = settings->value ("override_locale_val", "en").toString();
      if (ts.length() != 2)
         ts = "en";
      
      qtTranslator.load (QString ("qt_%1").arg (ts),
                         QLibraryInfo::location (QLibraryInfo::TranslationsPath));
      qApp->installTranslator (&qtTranslator);

      myappTranslator.load (":/translations/wavylon_" + ts);
      qApp->installTranslator (&myappTranslator);
     }
  else
      {
       qtTranslator.load (QString ("qt_%1").arg (QLocale::system().name()),
                          QLibraryInfo::location (QLibraryInfo::TranslationsPath));
       qApp->installTranslator (&qtTranslator);

       myappTranslator.load (":/translations/wavylon_" + QLocale::system().name());
       qApp->installTranslator (&myappTranslator);
      }

  createActions();
  createMenus();
  createToolBars();
  createStatusBar();

  init_styles();
  
  //update_sessions();
  update_palettes();
  update_themes();
  update_profiles();

  setMinimumSize (12, 12);

  project_manager = new CProjectManager;
  
  create_main_widget();

  idx_prev = 0;
  connect (main_tab_widget, SIGNAL(currentChanged(int)), this, SLOT(main_tab_page_changed(int)));

  readSettings();
  
   
  
  load_palette (fname_def_palette);

  
  shortcuts = new CShortcuts (this);
  shortcuts->fname.append (dir_config).append ("/shortcuts");
  shortcuts->load_from_file (shortcuts->fname);

  sl_fif_history = qstring_load (fname_fif).split ("\n");
  cmb_fif->addItems (sl_fif_history);
  cmb_fif->clearEditText(); 

  createFman();
  createOptions();
  createManual();

  dir_last = settings->value ("dir_last", QDir::homePath()).toString();
  b_preview = settings->value ("b_preview", false).toBool(); 
  
  l_status = new QLabel;
  pb_status = new QProgressBar;
  pb_status->setRange (0, 0);

  statusBar()->insertPermanentWidget (0, pb_status);
  statusBar()->insertPermanentWidget (1, l_status);

  pb_status->hide();
  
  restoreState (settings->value ("state", QByteArray()).toByteArray());

  text_file_browser = new QTextBrowser;
  text_file_browser->setWindowTitle (tr ("text browser"));
  text_file_browser->setReadOnly (true);
  text_file_browser->setOpenExternalLinks (true); 

  QString vn = settings->value ("VERSION_NUMBER", "").toString();
  if (vn.isEmpty() || vn != QString (VERSION_NUMBER))
      help_show_news();

  if (settings->value ("session_restore", false).toBool())
     {
      project_manager->project_open (settings->value ("last_session", "").toString());
      if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);

           
      if (project_manager->project)
          dir_last = get_file_path (project_manager->project->paths.fname_project);
     }

  
  
  
  handle_args();
  ui_update = false;
  
  setAcceptDrops (true);
  qApp->setWindowIcon (QIcon (":/icons/wavylon_icon.png"));
  log->log (tr ("<b>Wavylon %1 @ http://semiletov.org/wavylon</b><br>by Peter Semiletov (tea@list.ru)<br>read the Manual under the <i>learn</i> tab!").arg (QString (VERSION_NUMBER)));
  
  idx_tab_edit_activate();
    
  
  //dsp = new CDSP;
    
  
  /*
  QPoint ps = settings->value ("wnd_fxrack.pos", QPoint (pos().x() + width() + 3, y())).toPoint();
  
  if (settings->value ("wnd_fxrack_visible", true).toBool())
     {
      wnd_fxrack->move (ps);
      wnd_fxrack->show();
      wnd_fxrack->raise();
     } 
    */ 
  
  if (file_exists (settings->value ("temp_path", QDir::tempPath()).toString() + fname_tempfile))
     QFile::remove (settings->value ("temp_path", QDir::tempPath()).toString() + fname_tempfile);
   
}


void CWavylon::leaving_tune()
{
  ogg_q = spb_ogg_q->value();
  settings->setValue ("ogg_q", ogg_q);
  
  buffer_size_frames_multiplier = sp_buffer_size_frames_multiplier->value();
  
  settings->setValue ("buffer_size_frames_multiplier", (int) buffer_size_frames_multiplier);
  settings->setValue ("mp3_encode", ed_mp3_encode->text());
}


void CWavylon::closeEvent (QCloseEvent *event)
{
  
  pa_done();

  QString tfn = settings->value ("temp_path", QDir::tempPath()).toString() + fname_tempfile;
      
  //qDebug() << "tfn :" << tfn; 
      
  if (file_exists (tfn))
     {
    //  qDebug() << tfn; 
      QFile::remove (tfn );
     } 


  if (main_tab_widget->currentIndex() == idx_tab_tune)
     leaving_tune();

  writeSettings();

  qstring_save (fname_fif, sl_fif_history.join ("\n"));
  
  delete shortcuts;
  
//  delete dsp;
//  dsp = 0;
  
//  delete wnd_fxrack;
//  wnd_fxrack = 0;
  
  delete project_manager;
  project_manager = 0;
  
  file_formats_done();

  event->accept();
}


void CWavylon::newFile()
{
  
  //CREATE NEW PROJECT
  
  QString fileName = QFileDialog::getSaveFileName (this, tr ("New project"),
                                                   dir_last);
  
  
  project_manager->create_new_project (fileName);
  if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);


  main_tab_widget->setCurrentIndex (idx_tab_edit);
  
  
}


void CWavylon::open()
{
  qDebug() << "CEKO::open() - start";

  //wnd_fxrack->fx_rack->set_state_all (FXS_STOP);

/*  if (pa_stream)
     {
      Pa_AbortStream (pa_stream);	
      pa_stream = 0;
     }

  if (pa_stream_in)
     {
       Pa_AbortStream (pa_stream_in);	
       pa_stream_in = 0;
     }
*/

  if (! settings->value ("use_trad_dialogs", "0").toBool())
     {
      main_tab_widget->setCurrentIndex (idx_tab_fman);
      fm_entry_mode = FM_ENTRY_MODE_OPEN;

      return;
     }

 

  QFileDialog dialog (this);
  QSize size = settings->value ("dialog_size", QSize (width(), height())).toSize();
  dialog.resize (size);

  dialog.setFilter (QDir::AllEntries | QDir::Hidden);

  QList<QUrl> sidebarUrls = dialog.sidebarUrls();
  QList<QUrl> sidebarUrls_old = dialog.sidebarUrls();

  sidebarUrls.append (QUrl::fromLocalFile (dir_sessions));

#ifdef Q_QS_X11

  QDir volDir("/mnt");
  QStringList volumes (volDir.entryList (volDir.filter() | QDir::NoDotAndDotDot));

  foreach (QString v, volumes)  
          sidebarUrls.append (QUrl::fromLocalFile ("/mnt/" + v));

  QDir volDir2 ("/media");
  QStringList volumes2 (volDir2.entryList (volDir2.filter() | QDir::NoDotAndDotDot));

  foreach (QString v, volumes2)
          sidebarUrls.append (QUrl::fromLocalFile ("/media/" + v));

  
#endif

  dialog.setSidebarUrls (sidebarUrls);

  dialog.setFileMode (QFileDialog::ExistingFiles);
  dialog.setAcceptMode (QFileDialog::AcceptOpen);

  if (project_manager->project)
     {
      if (file_exists (project_manager->project->paths.fname_project))
          dialog.setDirectory (get_file_path (project_manager->project->paths.fname_project));
      else
          dialog.setDirectory (dir_last);
         
     }
  else
      dialog.setDirectory (dir_last);

  dialog.setNameFilter (tr ("All (*);;WAV files (*.wav);;Compressed files (*.ogg *.flac *.mp3)"));

  QStringList fileNames;
  
  if (dialog.exec())
     {
      dialog.setSidebarUrls (sidebarUrls_old);

      fileNames = dialog.selectedFiles();
      
      foreach (QString fn, fileNames)
              {
               project_manager->project_open (fn);
               
               if (project_manager->project)
                   transportToolBar->addWidget (project_manager->project->transport_simple);

               
               if (project_manager->project)
                   dir_last = get_file_path (project_manager->project->paths.fname_project);
              }
     }
  else
      dialog.setSidebarUrls (sidebarUrls_old);


  settings->setValue ("dialog_size", dialog.size());
  update_dyn_menus();

  //qDebug() << "CEKO::open() - end";
}


bool CWavylon::save()
{
  project_manager->project_save();
  return true;
}


bool CWavylon::saveAs()
{
/*  
  if (! settings->value ("use_trad_dialogs", "0").toBool())
     {
      main_tab_widget->setCurrentIndex (idx_tab_fman);
      fm_entry_mode = FM_ENTRY_MODE_SAVE;

      if (file_exists (d->file_name))
         fman->nav (get_file_path (d->file_name));
      else
          fman->nav (dir_last);

      ed_fman_fname->setFocus();   
          
      return true;
     }

  QFileDialog dialog (this);
  QSize size = settings->value ("dialog_size", QSize (width(), height())).toSize();
  dialog.resize (size);

  dialog.setFilter(QDir::AllEntries | QDir::Hidden);
  dialog.setOption (QFileDialog::DontUseNativeDialog, true);


  QList<QUrl> sidebarUrls = dialog.sidebarUrls();
  QList<QUrl> sidebarUrls_old = dialog.sidebarUrls();

  sidebarUrls.append(QUrl::fromLocalFile(dir_sessions));

#ifdef Q_QS_X11

  QDir volDir("/mnt");
  QStringList volumes (volDir.entryList (volDir.filter() | QDir::NoDotAndDotDot));

  foreach (QString v, volumes)
          sidebarUrls.append (QUrl::fromLocalFile ("/mnt/" + v));

  QDir volDir2 ("/media");
  QStringList volumes2 (volDir2.entryList (volDir2.filter() | QDir::NoDotAndDotDot));

  foreach (QString v, volumes2)
          sidebarUrls.append (QUrl::fromLocalFile ("/media/" + v));

#endif

  dialog.setSidebarUrls(sidebarUrls);

  dialog.setFileMode (QFileDialog::AnyFile);
  dialog.setAcceptMode (QFileDialog::AcceptSave);
  dialog.setConfirmOverwrite (false);
  dialog.setDirectory (dir_last);

  if (dialog.exec())
     {
      dialog.setSidebarUrls (sidebarUrls_old);

      QString fileName = dialog.selectedFiles().at(0);

      if (file_exists (fileName))
         {
          int ret = QMessageBox::warning (this, "Wavylon",
                                          tr ("%1 already exists\n"
                                          "Do you want to overwrite?")
                                           .arg (fileName),
                                          QMessageBox::Yes | QMessageBox::Default,
                                          QMessageBox::Cancel | QMessageBox::Escape);

          if (ret == QMessageBox::Cancel)
             return false;
         }

      d->save_with_name (fileName);
      
      update_dyn_menus();

      QFileInfo f (d->file_name);
      dir_last = f.path();
     }
   else
       dialog.setSidebarUrls (sidebarUrls_old);

  settings->setValue ("dialog_size", dialog.size());*/
  return true;
}


void CWavylon::about()
{
  CAboutWindow *a = new CAboutWindow();
  a->move (x() + 20, y() + 20);
  a->show();
}


void CWavylon::createActions()
{
  act_test = new QAction (QIcon (":/icons/file-save.png"), tr ("Test"), this);
  connect (act_test, SIGNAL(triggered()), this, SLOT(test()));

  newAct = new QAction (QIcon (":/icons/file-new.png"), tr ("New project"), this);
  newAct->setShortcut (QKeySequence ("Ctrl+N"));
  newAct->setStatusTip (tr ("Create the new project"));
  connect (newAct, SIGNAL(triggered()), this, SLOT(newFile()));

  QIcon ic_file_open (":/icons/file-open.png");
  ic_file_open.addFile (":/icons/file-open-active.png", QSize(), QIcon::Active); 

  openAct = new QAction (ic_file_open, tr ("Open file"), this);
  openAct->setStatusTip (tr ("Open an existing file"));
  connect (openAct, SIGNAL(triggered()), this, SLOT(open()));


  QIcon ic_file_save (":/icons/file-save.png");
  ic_file_save.addFile (":/icons/file-save-active.png", QSize(), QIcon::Active); 
  
  saveAct = new QAction (ic_file_save, tr ("Save"), this);
  saveAct->setShortcut (QKeySequence ("Ctrl+S"));
  saveAct->setStatusTip (tr ("Save the document to disk"));
  connect (saveAct, SIGNAL(triggered()), this, SLOT(save()));

  saveAsAct = new QAction (QIcon (":/icons/file-save-as.png"), tr ("Save As"), this);
  saveAsAct->setStatusTip (tr ("Save the document under a new name"));
  connect (saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

  exitAct = new QAction (tr ("Exit"), this);
  exitAct->setShortcut (QKeySequence ("Ctrl+Q"));
  exitAct->setStatusTip (tr ("Exit the application"));
  connect (exitAct, SIGNAL(triggered()), this, SLOT(close()));

  QIcon ic_edit_cut (":/icons/edit-cut.png");
  ic_edit_cut.addFile (":/icons/edit-cut-active.png", QSize(), QIcon::Active); 

  cutAct = new QAction (ic_edit_cut, tr ("Cut"), this);
  cutAct->setShortcut (QKeySequence ("Ctrl+X"));
  cutAct->setStatusTip (tr ("Cut the current selection's contents to the clipboard"));
  connect (cutAct, SIGNAL(triggered()), this, SLOT(ed_cut()));

  QIcon ic_edit_copy (":/icons/edit-copy.png");
  ic_edit_copy.addFile (":/icons/edit-copy-active.png", QSize(), QIcon::Active); 
  
  copyAct = new QAction (ic_edit_copy, tr("Copy"), this);
  copyAct->setShortcut (QKeySequence ("Ctrl+C"));
  copyAct->setStatusTip (tr ("Copy the current selection's contents to the clipboard"));
  connect (copyAct, SIGNAL(triggered()), this, SLOT(ed_copy()));


  QIcon ic_edit_paste (":/icons/edit-paste.png");
  ic_edit_paste.addFile (":/icons/edit-paste-active.png", QSize(), QIcon::Active); 

  pasteAct = new QAction (ic_edit_paste, tr("Paste"), this);
  pasteAct->setShortcut (QKeySequence ("Ctrl+V"));
  pasteAct->setStatusTip (tr ("Paste the clipboard's contents into the current selection"));
  connect (pasteAct, SIGNAL(triggered()), this, SLOT(ed_paste()));

  undoAct = new QAction (tr ("Undo"), this);
  undoAct->setShortcut (QKeySequence ("Ctrl+Z"));
  connect (undoAct, SIGNAL(triggered()), this, SLOT(ed_undo()));

  redoAct = new QAction (tr ("Redo"), this);
  connect (redoAct, SIGNAL(triggered()), this, SLOT(ed_redo()));

  aboutAct = new QAction (tr ("About"), this);
  connect (aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction (tr ("About Qt"), this);
  connect (aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    
  transport_play = new QAction (QIcon (":/icons/play.png"), tr ("Play/Pause (Space)"), this);
  transport_play->setStatusTip (tr ("Play/Pause"));
  //connect (transport_play, SIGNAL(triggered()), this, SLOT(slot_transport_play()));

  transport_stop = new QAction (QIcon (":/icons/stop.png"), tr ("Stop"), this);
  transport_stop->setStatusTip (tr ("Stop"));
 // connect (transport_stop, SIGNAL(triggered()), this, SLOT(slot_transport_stop()));
}


void CWavylon::createMenus()
{
  
  menu_project = menuBar()->addMenu (tr ("Project"));
  menu_project->setTearOffEnabled (true);

  //menu_project->addAction (act_test);

  menu_project->addAction (newAct);
  
  //add_to_menu (menu_project, tr ("Record"), SLOT(file_record()));
  add_to_menu (menu_project, tr ("Open"), SLOT(open()), "Ctrl+O", ":/icons/file-open.png");
  
  menu_project->addAction (saveAct);
  
  add_to_menu (menu_project, tr ("Last closed file"), SLOT(file_last_opened()));
  menu_file_recent = menu_project->addMenu (tr ("Recent files"));
  
  //fileMenu->addSeparator();
  

  add_to_menu (menu_project, tr ("WAVs"), SLOT(project_call_wavs_wnd()));
  add_to_menu (menu_project, tr ("Tracks"), SLOT(project_call_tracks_wnd()));
  
  add_to_menu (menu_project, tr ("Project settings"), SLOT(project_props()));
  add_to_menu (menu_project, tr ("Project mixdown"), SLOT(project_mixdown()));
  
  add_to_menu (menu_project, tr ("Close current"), SLOT(close_current()), "Ctrl+W");

  menu_project->addSeparator();
  
  menu_project->addAction (exitAct);
  
  
  
  
  menu_track = menuBar()->addMenu (tr ("Track"));
  menu_track->setTearOffEnabled (true);

  add_to_menu (menu_track, tr ("Create new WAV track"), SLOT(track_create_new()));
  add_to_menu (menu_track, tr ("Delete current track"), SLOT(track_delete()));
  add_to_menu (menu_track, tr ("Track up"), SLOT(track_up()));
  add_to_menu (menu_track, tr ("Track down"), SLOT(track_down()));



  menu_clip = menuBar()->addMenu (tr ("Clip"));
  menu_clip->setTearOffEnabled (true);

  add_to_menu (menu_clip, tr ("Edit clip"), SLOT(prj_clip_props()));
  

  editMenu = menuBar()->addMenu (tr ("Edit"));
  editMenu->setTearOffEnabled (true);

  editMenu->addAction (cutAct);
  editMenu->addAction (copyAct);
  editMenu->addAction (pasteAct);

  add_to_menu (editMenu, tr ("Delete (Del)"), SLOT(ed_delete()));
  //add_to_menu (editMenu, tr ("Trim"), SLOT(ed_trim()));
  
  editMenu->addSeparator();
  
  add_to_menu (editMenu, tr ("Select all"), SLOT(ed_select_all()), "Ctrl+A");
  add_to_menu (editMenu, tr ("Deselect"), SLOT(ed_deselect()));
  
  editMenu->addSeparator();


  editMenu->addAction (undoAct);
  //editMenu->addAction (redoAct);
  
  menu_search = menuBar()->addMenu (tr ("Search"));
  menu_search->setTearOffEnabled (true);

  add_to_menu (menu_search, tr ("Find"), SLOT(search_find()));
  add_to_menu (menu_search, tr ("Find next"), SLOT(search_find_next()),"F3");
  add_to_menu (menu_search, tr ("Find previous"), SLOT(search_find_prev()),"Ctrl+F3");

  
  //menu_functions = menuBar()->addMenu (tr ("Functions"));
  //menu_functions->setTearOffEnabled (true);


  
  menu_nav = menuBar()->addMenu (tr ("Nav"));
  menu_nav->setTearOffEnabled (true);

  menu_nav->addAction (transport_play);
  menu_nav->addAction (transport_stop);

  add_to_menu (menu_nav, tr ("Play looped on/off"), SLOT(nav_play_looped()));

  menu_nav->addSeparator();
  

  add_to_menu (menu_nav, tr ("Focus the Famous input field"), SLOT(nav_focus_to_fif()), "Ctrl+F");
  //add_to_menu (menu_nav, tr ("Focus the editor"), SLOT(nav_focus_to_editor()));

  menu_fm = menuBar()->addMenu (tr ("Fm"));
  menu_fm->setTearOffEnabled (true);

  menu_fm_file_ops = menu_fm->addMenu (tr ("File operations"));
  menu_fm_file_ops->setTearOffEnabled (true);

  add_to_menu (menu_fm_file_ops, tr ("Create new directory"), SLOT(fman_create_dir()));
  add_to_menu (menu_fm_file_ops, tr ("Rename"), SLOT(fman_rename()));
  add_to_menu (menu_fm_file_ops, tr ("Delete file"), SLOT(fman_delete()));

  menu_fm_file_infos = menu_fm->addMenu (tr ("File information"));
  menu_fm_file_infos->setTearOffEnabled (true);

  //add_to_menu (menu_fm_file_infos, tr ("Full info"), SLOT(fm_full_info()));

  
  add_to_menu (menu_fm, tr ("Go to home dir"), SLOT(fman_home()));
  add_to_menu (menu_fm, tr ("Refresh current dir"), SLOT(fman_refresh()));

  add_to_menu (menu_fm, tr ("Select by regexp"), SLOT(fman_select_by_regexp()));
  add_to_menu (menu_fm, tr ("Deselect by regexp"), SLOT(fman_deselect_by_regexp()));
  
  add_to_menu (menu_fm, tr ("Project WAVs"), SLOT(fman_project_files()));
  
  
  
  
  menu_view = menuBar()->addMenu (tr ("View"));
  menu_view->setTearOffEnabled (true);

  menu_view_palettes = menu_view->addMenu (tr ("Palettes"));
  menu_view_palettes->setTearOffEnabled (true);

  menu_view_themes = menu_view->addMenu (tr ("Themes"));
  menu_view_themes->setTearOffEnabled (true);



  menu_view_profiles = menu_view->addMenu (tr ("Profiles"));
  menu_view_profiles->setTearOffEnabled (true);

  add_to_menu (menu_view, tr ("Save profile"), SLOT(profile_save_as()));

  //add_to_menu (menu_view, tr ("Show/hide FX Rack"), SLOT(view_show_mixer()), "Alt+M");
  
  add_to_menu (menu_view, tr ("Toggle fullscreen"), SLOT(view_toggle_fs()));
  add_to_menu (menu_view, tr ("Stay on top"), SLOT(view_stay_on_top()));


  helpMenu = menuBar()->addMenu (tr ("Help"));
  helpMenu->setTearOffEnabled (true);

  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);
  add_to_menu (helpMenu, tr ("NEWS"), SLOT(help_show_news()));
  add_to_menu (helpMenu, "TODO", SLOT(help_show_todo()));
  add_to_menu (helpMenu, "ChangeLog", SLOT(help_show_changelog()));
  add_to_menu (helpMenu, tr ("License"), SLOT(help_show_gpl()));
}


void CWavylon::createToolBars()
{
  openAct->setMenu (menu_file_recent);

  fileToolBar = addToolBar (tr ("File"));
  fileToolBar->setObjectName ("fileToolBar");
  fileToolBar->addAction (newAct);
  fileToolBar->addAction (openAct);
  fileToolBar->addAction (saveAct);

  editToolBar = addToolBar (tr ("Edit"));
  editToolBar->setObjectName ("editToolBar");
  editToolBar->addAction (cutAct);
  editToolBar->addAction (copyAct);
  editToolBar->addAction (pasteAct);
  
  transportToolBar = addToolBar (tr ("Transport"));
  transportToolBar->setObjectName ("transportToolBar");
  
  cb_play_looped = new QCheckBox (tr ("looped")); 
  connect(cb_play_looped, SIGNAL(stateChanged (int )), this, SLOT(cb_play_looped_changed (int )));
  transportToolBar->addWidget (cb_play_looped);
   
    
  QLabel *lt = new QLabel (tr ("Current time: "));
  l_maintime = new QLabel; 

 // transportToolBar->addWidget (lt);
  //transportToolBar->addWidget (l_maintime);
}


void CWavylon::createStatusBar()
{
  statusBar()->showMessage (tr ("Ready"));
}


CWavylon::~CWavylon()
{
//  qDebug() << "~CEKO()";

  delete avail_fx;

  QFile::remove (temp_mp3_fname);

  delete text_file_browser;
  delete fman;
  delete log;
 // delete transport_control;
}


void CWavylon::pageChanged (int index)
{
  //qDebug() << "CEKO::pageChanged index = " << index;
/*
  transport_state = STATE_STOP;
  
  if (pa_stream)
     {
      Pa_CloseStream (pa_stream);	
      pa_stream = 0;
     }

  if (pa_stream_in)
     {
       Pa_CloseStream (pa_stream_in);	
       pa_stream_in = 0;
     }


  if (index == -1)
     {
      documents->current = 0;
      return;  
     }
  
  documents->current = documents->list[index];
  
  if (documents->current)
     {
      documents->current->update_title();
      cb_play_looped->setChecked (documents->current->wave_edit->waveform->play_looped);
     } 
*/
  //qDebug() << "CEKO::pageChanged end" << index;
}


void CWavylon::close_current()
{
/*  transport_state = STATE_STOP;
  
  if (pa_stream)
     {
      Pa_CloseStream (pa_stream);	
      pa_stream = 0;
     }
  */   
}


void CWavylon::ed_copy()
{
  if (main_tab_widget->currentIndex() == idx_tab_edit)
     {
     }
  else
      if (main_tab_widget->currentIndex() == idx_tab_learn)      
          man->copy();
}


void CWavylon::ed_cut()
{
}


void CWavylon::ed_paste()
{
}


void CWavylon::ed_undo()
{
}


void CWavylon::ed_redo()
{
}


QAction* CWavylon::add_to_menu (QMenu *menu,
                            const QString &caption,
                            const char *method,
                            const QString &shortkt,
                            const QString &iconpath
                           )
{
  QAction *act = new QAction (caption, this);

  if (! shortkt.isEmpty())
     act->setShortcut (shortkt);

  if (! iconpath.isEmpty())
     act->setIcon (QIcon (iconpath));

  connect (act, SIGNAL(triggered()), this, method);
  menu->addAction (act);
  return act;
}


void CWavylon::search_find()
{
  if (main_tab_widget->currentIndex() == idx_tab_learn)
      man_find_find();
  else
      if (main_tab_widget->currentIndex() == idx_tab_tune)
         opt_shortcuts_find();
  else
      if (main_tab_widget->currentIndex() == idx_tab_fman)
         fman_find();
}


void CWavylon::fman_find()
{
  QString ft = fif_get_text();
  if (ft.isEmpty()) 
      return;  
     
  l_fman_find = fman->mymodel->findItems (ft, Qt::MatchStartsWith);
  
  if (l_fman_find.size() < 1)
     return; 
  
  fman_find_idx = 0;
  
  fman->setCurrentIndex (fman->mymodel->indexFromItem (l_fman_find[fman_find_idx]));
}


void CWavylon::fman_find_next()
{
  QString ft = fif_get_text();
  if (ft.isEmpty()) 
      return;  
   
  if (l_fman_find.size() < 1)
     return; 
  
  if (fman_find_idx < (l_fman_find.size() - 1))
     fman_find_idx++;
  
  fman->setCurrentIndex (fman->mymodel->indexFromItem (l_fman_find[fman_find_idx]));
}


void CWavylon::fman_find_prev()
{
  QString ft = fif_get_text();
  if (ft.isEmpty()) 
      return;  
   
  if (l_fman_find.size() < 1)
     return; 
  
  if (fman_find_idx != 0)
     fman_find_idx--;
  
  fman->setCurrentIndex (fman->mymodel->indexFromItem (l_fman_find[fman_find_idx]));
}


void CWavylon::search_find_next()
{
  if (main_tab_widget->currentIndex() == idx_tab_learn)
      man_find_next();
  else
      if (main_tab_widget->currentIndex() == idx_tab_tune)
          opt_shortcuts_find_next();  
  else
      if (main_tab_widget->currentIndex() == idx_tab_fman)
          fman_find_next();
}


void CWavylon::search_find_prev()
{
  if (main_tab_widget->currentIndex() == idx_tab_learn)
          man_find_prev();
  else
      if (main_tab_widget->currentIndex() == idx_tab_tune)
         opt_shortcuts_find_prev();  
  else
      if (main_tab_widget->currentIndex() == idx_tab_fman)
         fman_find_prev();  
}


void CWavylon::opt_shortcuts_find()
{
  opt_shortcuts_string_to_find = fif_get_text();
  
  int from = lv_menuitems->currentRow();
  
  if (from == -1)
     from = 0;
  
  int index = shortcuts->captions.indexOf (QRegExp (opt_shortcuts_string_to_find + ".*", Qt::CaseInsensitive), from);
  if (index != -1) 
     lv_menuitems->setCurrentRow (index);
}


void CWavylon::opt_shortcuts_find_next()
{
  int from = lv_menuitems->currentRow();
  if (from == -1)
     from = 0;
  
  int index = shortcuts->captions.indexOf (QRegExp (opt_shortcuts_string_to_find + ".*", Qt::CaseInsensitive), from + 1);
  if (index != -1) 
    lv_menuitems->setCurrentRow (index);
}


void CWavylon::opt_shortcuts_find_prev()
{
  int from = lv_menuitems->currentRow();
  if (from == -1)
     from = 0;
  
  int index = shortcuts->captions.lastIndexOf (QRegExp (opt_shortcuts_string_to_find + ".*", Qt::CaseInsensitive), from - 1);
  if (index != -1) 
     lv_menuitems->setCurrentRow (index); 
}


void CWavylon::slot_lv_menuitems_currentItemChanged (QListWidgetItem *current, QListWidgetItem *previous)
{
  QAction *a = shortcuts->find_by_caption (current->text());
  if (a)
     ent_shtcut->setText (a->shortcut().toString());
}


void CWavylon::pb_assign_hotkey_clicked()
{
  if (! lv_menuitems->currentItem())
     return;

  if (ent_shtcut->text().isEmpty())
     return;

  shortcuts->set_new_shortcut (lv_menuitems->currentItem()->text(), ent_shtcut->text());
  shortcuts->save_to_file (shortcuts->fname);
}


void CWavylon::pb_remove_hotkey_clicked ()
{
  if (! lv_menuitems->currentItem())
     return;

  shortcuts->set_new_shortcut (lv_menuitems->currentItem()->text(), "");
  ent_shtcut->setText ("");
  shortcuts->save_to_file (shortcuts->fname);
}


void CWavylon::slot_sl_icons_size_sliderMoved (int value)
{
  setIconSize (QSize (value, value));
  tb_fman_dir->setIconSize (QSize (value, value));
}


void CWavylon::ed_locale_override_editingFinished()
{
  settings->setValue ("override_locale_val", ed_locale_override->text());
}


void CWavylon::cmb_sound_dev_out_currentIndexChanged (int index)
{
  settings->setValue ("sound_dev_id_out", index);
  pa_device_id_out = index;
}


void CWavylon::cmb_mono_recording_mode_currentIndexChanged (int index)
{
  settings->setValue ("mono_recording_mode", index);
  mono_recording_mode = index;
}


void CWavylon::cmb_sound_dev_in_currentIndexChanged (int index)
{
  settings->setValue ("sound_dev_id_in", index);
  pa_device_id_in = index;
}


void CWavylon::cmb_panner_currentIndexChanged (int index)
{
  settings->setValue ("panner", index);
  //dsp->panner = index;
}


void CWavylon::cmb_proxy_video_decoder_currentIndexChanged (int index)
{
  settings->setValue ("proxy_video_decoder", index);
  proxy_video_decoder = index;
}


void CWavylon::pb_choose_temp_path_clicked()
{

  QString path = QFileDialog::getExistingDirectory (this, tr ("Open Directory"), "/",
                                                    QFileDialog::ShowDirsOnly |
                                                    QFileDialog::DontResolveSymlinks);
  if (! path.isEmpty())
    {
     settings->setValue ("temp_path", path);
     ed_temp_path->setText (path);
    }
}



void CWavylon::createOptions()
{
  tab_options = new QTabWidget;

  idx_tab_tune = main_tab_widget->addTab (tab_options, tr ("tune"));
  
  QWidget *page_interface = new QWidget (tab_options);
  QHBoxLayout *lt_h = new QHBoxLayout;

  QComboBox *cmb_styles = new QComboBox (page_interface);
  cmb_styles->addItems (QStyleFactory::keys());
 
  connect (cmb_styles, SIGNAL(currentIndexChanged (const QString &)),
           this, SLOT(slot_style_currentIndexChanged (const QString &)));

  QLabel *l_app_font = new QLabel (tr ("Interface font"));

  cmb_app_font_name = new QFontComboBox (page_interface);
  cmb_app_font_name->setCurrentFont (QFont (settings->value ("app_font_name", qApp->font().family()).toString()));
  spb_app_font_size = new QSpinBox (page_interface);
  spb_app_font_size->setRange (6, 64);
  QFontInfo fi = QFontInfo (qApp->font());

  spb_app_font_size->setValue (settings->value ("app_font_size", fi.pointSize()).toInt());
  connect (spb_app_font_size, SIGNAL(valueChanged (int)), this, SLOT(slot_app_font_size_changed (int )));

  connect (cmb_app_font_name, SIGNAL(currentIndexChanged ( const QString & )),
           this, SLOT(slot_app_fontname_changed(const QString & )));


  QHBoxLayout *hb_icon_size = new QHBoxLayout;

  QLabel *l_icons_size = new QLabel (tr ("Icons size"));

  QStringList sl_icon_sizes;

  sl_icon_sizes.append ("16");
  sl_icon_sizes.append ("24");
  sl_icon_sizes.append ("32");
  sl_icon_sizes.append ("64");

  QComboBox *cmb_icon_size = new QComboBox;
  cmb_icon_size->addItems (sl_icon_sizes);

  connect (cmb_icon_size, SIGNAL(currentIndexChanged (const QString &)),
           this, SLOT(cmb_icon_sizes_currentIndexChanged (const QString &)));

  cmb_icon_size->setCurrentIndex (sl_icon_sizes.indexOf (settings->value ("icon_size", "32").toString()));

  hb_icon_size->addWidget (l_icons_size);
  hb_icon_size->addWidget (cmb_icon_size);


  QLabel *l_uistyle = new QLabel (tr ("UI style"));
  cmb_styles->setCurrentIndex (cmb_styles->findText (settings->value ("ui_style", "Fusion").toString()));

    
  QCheckBox *cb_show_meterbar_in_db = new QCheckBox (tr ("Amplitude meter bar in dB"), tab_options);
  cb_show_meterbar_in_db->setCheckState (Qt::CheckState (settings->value ("meterbar_show_db", "1").toInt()));
  connect(cb_show_meterbar_in_db, SIGNAL(stateChanged (int )), this, SLOT(cb_show_meterbar_in_db_changed (int )));

  QCheckBox *cb_use_trad_dialogs = new QCheckBox (tr ("Use traditional File Save/Open dialogs"), tab_options);
  cb_use_trad_dialogs->setCheckState (Qt::CheckState (settings->value ("use_trad_dialogs", "0").toInt()));
  connect (cb_use_trad_dialogs, SIGNAL(stateChanged (int )), this, SLOT(cb_use_trad_dialogs_changed (int )));


  QVBoxLayout *page_interface_layout = new QVBoxLayout;
  page_interface_layout->setAlignment (Qt::AlignTop);

  lt_h = new QHBoxLayout;

  lt_h->addWidget (l_app_font);
  lt_h->addWidget (cmb_app_font_name);
  lt_h->addWidget (spb_app_font_size);

  page_interface_layout->addLayout (lt_h);

  page_interface_layout->addLayout (hb_icon_size);

  lt_h = new QHBoxLayout;

  lt_h->addWidget (l_uistyle);
  lt_h->addWidget (cmb_styles);
 
   cb_altmenu = new QCheckBox (tr ("Use Alt key to access main menu"), tab_options);
  if (b_altmenu)
    cb_altmenu->setCheckState (Qt::Checked);
  else
      cb_altmenu->setCheckState (Qt::Unchecked);
  
   connect (cb_altmenu, SIGNAL(stateChanged (int)),
           this, SLOT(cb_altmenu_stateChanged (int)));

   
  page_interface_layout->addLayout (lt_h);
  page_interface_layout->addWidget (cb_show_meterbar_in_db); 
  page_interface_layout->addWidget (cb_use_trad_dialogs);
  page_interface_layout->addWidget (cb_altmenu);
 
 
  page_interface->setLayout (page_interface_layout);
  page_interface->show();

  tab_options->addTab (page_interface, tr ("Interface"));

  //////////
  
  QWidget *page_common = new QWidget (tab_options);
  QVBoxLayout *page_common_layout = new QVBoxLayout;
  page_common_layout->setAlignment (Qt::AlignTop);
  
  
  
  QHBoxLayout *hb_temp_path = new QHBoxLayout;
  QLabel *l_t = new QLabel (tr ("Temp directory"));

  ed_temp_path = new QLineEdit (this);
  ed_temp_path->setText (settings->value ("temp_path", QDir::tempPath()).toString());
  ed_temp_path->setReadOnly (true);

  QPushButton *pb_choose_temp_path = new QPushButton (tr ("Select"), this);

  connect (pb_choose_temp_path, SIGNAL(clicked()), this, SLOT(pb_choose_temp_path_clicked()));

  hb_temp_path->addWidget (l_t);
  hb_temp_path->addWidget (ed_temp_path);
  hb_temp_path->addWidget (pb_choose_temp_path);

  page_common_layout->addLayout (hb_temp_path);
  
  

  QPushButton *bt_set_def_format = new QPushButton (tr ("Set default format for new files"));
  //connect (bt_set_def_format, SIGNAL(clicked()), this, SLOT(bt_set_def_format_clicked()));

  QHBoxLayout *hb_mp3_encode = new QHBoxLayout;
  QLabel *l_mp3_encode = new QLabel (tr ("MP3 encode command"));
  ed_mp3_encode = new QLineEdit;
  ed_mp3_encode->setText (settings->value ("mp3_encode", "lame -b 320 -q 0 \"@FILEIN\" \"@FILEOUT\"").toString());
  

  hb_mp3_encode->addWidget (l_mp3_encode);  
  hb_mp3_encode->addWidget (ed_mp3_encode);  


  QHBoxLayout *lt_ogg_q = new QHBoxLayout;

  QLabel *l_ogg_q = new QLabel (tr ("Ogg vorbis quality on saving"));
  spb_ogg_q = new QDoubleSpinBox;
  spb_ogg_q->setRange (0, 1);
  spb_ogg_q->setSingleStep (0.1);
  spb_ogg_q->setValue (settings->value ("ogg_q", 0.5).toDouble());

  lt_ogg_q->addWidget (l_ogg_q);
  lt_ogg_q->addWidget (spb_ogg_q);





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
  
  cmb_panner->setCurrentIndex (settings->value ("panner", 0).toInt());
  
  
  connect (cmb_panner, SIGNAL(currentIndexChanged (int)),
           this, SLOT(cmb_panner_currentIndexChanged (int)));



 
  QHBoxLayout *h_proxy_video_decoder = new QHBoxLayout;
  QLabel *l_proxy_video_decoder = new QLabel (tr ("MP3 and video decoder (restart Wavylon to apply):"));
    
  QComboBox *cmb_proxy_video_decoder = new QComboBox;
  
  QStringList sl_video_decoders;
  sl_video_decoders.append ("FFMPEG");
  sl_video_decoders.append ("MPlayer");
  
  cmb_proxy_video_decoder->addItems (sl_video_decoders);
  
  h_proxy_video_decoder->addWidget (l_proxy_video_decoder);
  h_proxy_video_decoder->addWidget (cmb_proxy_video_decoder);
  
  cmb_proxy_video_decoder->setCurrentIndex (settings->value ("proxy_video_decoder", 0).toInt());
  
  
  connect (cmb_proxy_video_decoder, SIGNAL(currentIndexChanged (int)),
           this, SLOT(cmb_proxy_video_decoder_currentIndexChanged (int)));


  QHBoxLayout *lt_maxundos = new QHBoxLayout;
 
  QLabel *l_maxundos = new QLabel (tr ("Max undo items per file"));
  QSpinBox *spb_max_undos = new QSpinBox;
  spb_max_undos->setValue (settings->value ("max_undos", 6).toInt());
  spb_max_undos->setMinimum (1);
  connect (spb_max_undos, SIGNAL(valueChanged (int )), this, SLOT(spb_max_undos_valueChanged (int )));

  lt_maxundos->addWidget (l_maxundos);
  lt_maxundos->addWidget (spb_max_undos);

  QCheckBox *cb_session_restore = new QCheckBox (tr ("Restore the last session on start-up"), tab_options);
  cb_session_restore->setCheckState (Qt::CheckState (settings->value ("session_restore", "0").toInt()));
  connect(cb_session_restore, SIGNAL(stateChanged (int )), this, SLOT(cb_session_restore (int )));
  
  QCheckBox *cb_override_locale = new QCheckBox (tr ("Override locale"), tab_options);
  cb_override_locale->setCheckState (Qt::CheckState (settings->value ("override_locale", 0).toInt()));
  connect(cb_override_locale, SIGNAL(stateChanged (int )), this, SLOT(cb_locale_override (int )));

  ed_locale_override = new QLineEdit (this);
  ed_locale_override->setText (settings->value ("override_locale_val", "en").toString());
  connect(ed_locale_override, SIGNAL(editingFinished()), this, SLOT(ed_locale_override_editingFinished()));

  QHBoxLayout *hb_locovr = new QHBoxLayout;
  
  hb_locovr->addWidget (cb_override_locale);
  hb_locovr->addWidget (ed_locale_override);
  
  page_common_layout->addWidget (bt_set_def_format);

  page_common_layout->addLayout (h_proxy_video_decoder);

  page_common_layout->addLayout (hb_mp3_encode);

  page_common_layout->addLayout (lt_ogg_q);

  page_common_layout->addLayout (lt_maxundos);
  
  page_common_layout->addLayout (h_panner);
      
  page_common_layout->addWidget (cb_session_restore);
    
  page_common_layout->addLayout (hb_locovr);
    
  
  
  page_common->setLayout (page_common_layout);
  page_common->show();

  tab_options->addTab (page_common, tr ("Common"));

  
/////////////

  QWidget *page_soundev = new QWidget (tab_options);
  QVBoxLayout *page_soundev_layout = new QVBoxLayout;
  page_soundev_layout->setAlignment (Qt::AlignTop);

  page_soundev->setLayout (page_soundev_layout);
  page_soundev->show();

  tab_options->addTab (page_soundev, tr ("Sound devices"));


  QHBoxLayout *hb_soundev = new QHBoxLayout;

  pa_device_id_out = settings->value ("sound_dev_id_out", Pa_GetDefaultOutputDevice()).toInt();
  pa_device_id_in = settings->value ("sound_dev_id_in", Pa_GetDefaultInputDevice()).toInt();
  
  QLabel *l_soundev = new QLabel (tr ("Output"));
  
  cmb_sound_dev_out = new QComboBox;
  cmb_sound_dev_out->addItems (get_sound_devices());
  
  hb_soundev->addWidget (l_soundev);
  hb_soundev->addWidget (cmb_sound_dev_out);



  QHBoxLayout *hb_soundev_in = new QHBoxLayout;

  QLabel *l_soundev_in = new QLabel (tr ("Input"));

  cmb_sound_dev_in = new QComboBox;
  cmb_sound_dev_in->addItems (get_sound_devices());
  
  hb_soundev_in->addWidget (l_soundev_in);
  hb_soundev_in->addWidget (cmb_sound_dev_in);

  
  cmb_sound_dev_in->setCurrentIndex (pa_device_id_in);
  cmb_sound_dev_out->setCurrentIndex (pa_device_id_out);
  
  
  connect (cmb_sound_dev_in, SIGNAL(currentIndexChanged (int)),
           this, SLOT(cmb_sound_dev_in_currentIndexChanged (int)));

  connect (cmb_sound_dev_out, SIGNAL(currentIndexChanged (int)),
           this, SLOT(cmb_sound_dev_out_currentIndexChanged (int)));


  QHBoxLayout *hb_mono_recording_mode = new QHBoxLayout;

  QLabel *l_mono_recording_mode = new QLabel (tr ("Mono recording mode"));

  QComboBox *cmb_mono_recording_mode = new QComboBox;
  
  QStringList lmrm;
  
  lmrm.append (tr ("Use left channel"));
  lmrm.append (tr ("Use right channel"));
  
  cmb_mono_recording_mode->addItems (lmrm);
  
  hb_mono_recording_mode->addWidget (l_mono_recording_mode);
  hb_mono_recording_mode->addWidget (cmb_mono_recording_mode);

  connect (cmb_mono_recording_mode, SIGNAL(currentIndexChanged (int)),
           this, SLOT(cmb_mono_recording_mode_currentIndexChanged (int)));
  
  cmb_mono_recording_mode->setCurrentIndex (settings->value ("mono_recording_mode", 0).toInt());


  QHBoxLayout *hb_buffer_size_frames = new QHBoxLayout;

  QLabel *l_buffer_size_frames = new QLabel (tr ("Buffer size (in frames)"));
  
  cmb_buffer_size_frames = new QComboBox;
  
  QStringList lsizes;
  
  lsizes.append ("128");
  lsizes.append ("256");
  lsizes.append ("512");
  lsizes.append ("1024");
  lsizes.append ("2048");
  lsizes.append ("4096");
  
  cmb_buffer_size_frames->addItems (lsizes);
  
  
  hb_buffer_size_frames->addWidget (l_buffer_size_frames);
  hb_buffer_size_frames->addWidget (cmb_buffer_size_frames);

  int idx = lsizes.indexOf (settings->value ("buffer_size_frames", "2048").toString());

  cmb_buffer_size_frames->setCurrentIndex (idx);
 
  
  connect (cmb_buffer_size_frames, SIGNAL(currentIndexChanged (const QString &)),
           this, SLOT(cmb_buffer_size_frames_currentIndexChanged (const QString &)));
  
  
  QHBoxLayout *hb_buffer_size_frames_multiplier = new QHBoxLayout;

  QLabel *l_buffer_size_frames_multiplier = new QLabel (tr ("Buffer size multiplier"));
  sp_buffer_size_frames_multiplier = new QSpinBox;
  
  sp_buffer_size_frames_multiplier->setMinimum (1);
  sp_buffer_size_frames_multiplier->setMaximum (4096);
  sp_buffer_size_frames_multiplier->setValue (buffer_size_frames_multiplier);
  
  
  hb_buffer_size_frames_multiplier->addWidget (l_buffer_size_frames_multiplier);
  hb_buffer_size_frames_multiplier->addWidget (sp_buffer_size_frames_multiplier);

  
  
  QCheckBox *cb_monitor_input = new QCheckBox (tr ("Monitor input"), tab_options);
  cb_monitor_input->setCheckState (Qt::CheckState (settings->value ("b_monitor_input", 0).toInt()));
  connect(cb_monitor_input, SIGNAL(stateChanged (int )), this, SLOT(cb_monitor_input_changed (int )));

  
  
  
  page_soundev_layout->addLayout (hb_soundev_in);
  page_soundev_layout->addLayout (hb_soundev);
  page_soundev_layout->addLayout (hb_mono_recording_mode);

  page_soundev_layout->addLayout (hb_buffer_size_frames);
  page_soundev_layout->addLayout (hb_buffer_size_frames_multiplier);
    
  
  page_soundev_layout->addWidget (cb_monitor_input);
  

//////////

  QWidget *page_keyboard = new QWidget (tab_options);

  lt_h = new QHBoxLayout;

  QVBoxLayout *lt_vkeys = new QVBoxLayout;
  QVBoxLayout *lt_vbuttons = new QVBoxLayout;

  lv_menuitems = new QListWidget;
  opt_update_keyb();
  
  lt_vkeys->addWidget (lv_menuitems);

  connect (lv_menuitems, SIGNAL(currentItemChanged (QListWidgetItem *, QListWidgetItem *)),
           this, SLOT(slot_lv_menuitems_currentItemChanged (QListWidgetItem *, QListWidgetItem *)));

  ent_shtcut = new CShortcutEntry;
  lt_vbuttons->addWidget (ent_shtcut);

  QPushButton *pb_assign_hotkey = new QPushButton (tr ("Assign"), this);
  QPushButton *pb_remove_hotkey = new QPushButton (tr ("Remove"), this);

  connect (pb_assign_hotkey, SIGNAL(clicked()), this, SLOT(pb_assign_hotkey_clicked()));
  connect (pb_remove_hotkey, SIGNAL(clicked()), this, SLOT(pb_remove_hotkey_clicked()));

  lt_vbuttons->addWidget (pb_assign_hotkey);
  lt_vbuttons->addWidget (pb_remove_hotkey, 0, Qt::AlignTop);

  lt_h->addLayout (lt_vkeys);
  lt_h->addLayout (lt_vbuttons);

  page_keyboard->setLayout (lt_h);
  page_keyboard->show();

  tab_options->addTab (page_keyboard, tr ("Keyboard"));
}


void CWavylon::opt_update_keyb()
{
   if (! lv_menuitems)
      return;
   
   shortcuts->captions_iterate();
   lv_menuitems->clear();
   lv_menuitems->addItems (shortcuts->captions);
}


void CWavylon::slot_style_currentIndexChanged (const QString &text)
{
  /*if (text == "GTK+") //sorry, it causes bugs with EKO
     return;

  QApplication::setStyle (QStyleFactory::create (text));
  QApplication::setStyle (new MyProxyStyle);
  
  settings->setValue ("ui_style", text);*/
   if (text == "GTK+") //because it is buggy with some Qt versions. sorry!
     return;
/*
  QApplication::setStyle (QStyleFactory::create (text));
  
  QApplication::setStyle (new MyProxyStyle);
  */
  
  MyProxyStyle *ps = new MyProxyStyle (QStyleFactory::create (text));
  
  QApplication::setStyle (ps);

  settings->setValue ("ui_style", text);
}


void CWavylon::cb_session_restore (int state)
{
  settings->setValue ("session_restore", state);
}


void CWavylon::cb_use_trad_dialogs_changed (int state)
{
  settings->setValue ("use_trad_dialogs", state);
}


void CWavylon::cb_locale_override (int state)
{
  settings->setValue ("override_locale", state);
}


void CWavylon::cb_monitor_input_changed (int state)
{
  settings->setValue ("b_monitor_input", state);
  b_monitor_input = bool (state);
}


void CWavylon::slot_app_fontname_changed (const QString &text)
{
  settings->setValue ("app_font_name", text);
  update_stylesheet (fname_stylesheet);
}


void CWavylon::slot_app_font_size_changed (int i)
{
  settings->setValue("app_font_size", i);
  update_stylesheet (fname_stylesheet);
}


void CWavylon::updateFonts()
{
  //documents->apply_settings();

  QFont fapp;
  QFontInfo fi = QFontInfo (qApp->font());

  fapp.setPointSize (settings->value ("app_font_size", fi.pointSize()).toInt());
  fapp.fromString (settings->value ("app_font_name", qApp->font().family()).toString());
  qApp->setFont (fapp);
}


void CWavylon::man_find_find()
{
  man_search_value = fif_get_text();
  man->find (man_search_value);
}


void CWavylon::man_find_next()
{
  man->find (man_search_value);
}


void CWavylon::man_find_prev()
{
//  QTextDocument::FindFlags flags = QTextDocument::FindBackward;
  man->find (man_search_value, QTextDocument::FindBackward);
}


void CWavylon::createManual()
{
  QWidget *wd_man = new QWidget (this);

  QVBoxLayout *lv_t = new QVBoxLayout;

  QString loc = QLocale::system().name().left (2);

  if (settings->value ("override_locale", 0).toBool())
     {
      QString ts = settings->value ("override_locale_val", "en").toString();
      if (ts.length() != 2)
          ts = "en";
      loc = ts;
     }

  QString filename (":/manuals/");
  filename.append (loc).append (".html");
  
  if (! file_exists (filename))
      filename = ":/manuals/en.html";

  man_search_value = "";

  QHBoxLayout *lh_controls = new QHBoxLayout();

  QPushButton *bt_back = new QPushButton ("<");
  QPushButton *bt_forw = new QPushButton (">");

  lh_controls->addWidget (bt_back);
  lh_controls->addWidget (bt_forw);

  man = new QTextBrowser;
  man->setOpenExternalLinks (true);
  man->setSource (QUrl ("qrc" + filename));

  connect (bt_back, SIGNAL(clicked()), man, SLOT(backward()));
  connect (bt_forw, SIGNAL(clicked()), man, SLOT(forward()));

  lv_t->addLayout (lh_controls);
  lv_t->addWidget (man);

  wd_man->setLayout (lv_t);

  idx_tab_learn = main_tab_widget->addTab (wd_man, tr ("learn"));
}


void CWavylon::file_last_opened()
{/*
  if (documents->recent_files.size() > 0)
     {
      documents->open_file (documents->recent_files[0]);
      documents->recent_files.removeAt (0);
      documents->update_recent_menu();
      main_tab_widget->setCurrentIndex (idx_tab_edit); 
     }*/
}


void CWavylon::file_save_version()
{
  /*CDocument *d = documents->get_current();
  if (! d)
     return;

  if (! file_exists (d->file_name))
     return;

  QDate date = QDate::currentDate();
  QFileInfo fi;
  fi.setFile (d->file_name);

  QString version_timestamp_fmt = settings->value ("version_timestamp_fmt", "yyyy-MM-dd").toString();
  QTime t = QTime::currentTime();

  QString fname = QString ("%1/%2-%3-%4.%5").arg (
                           fi.absoluteDir().absolutePath()).arg (
                           date.toString (version_timestamp_fmt)).arg (
                           t.toString("hh-mm-ss")).arg (
                           fi.baseName()).arg (
                           fi.suffix());

  if (d->save_with_name_plain (fname))
     log->log (tr ("%1 - saved").arg (fname));
  else
     log->log (tr ("Cannot save %1").arg (fname));*/
}


void CWavylon::dragEnterEvent (QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat ("text/uri-list"))
     event->acceptProposedAction();
}


void CWavylon::dropEvent (QDropEvent *event)
{
  /*QString fName;
  QFileInfo info;
  
  if (! event->mimeData()->hasUrls())
     return;
     
  foreach (QUrl u, event->mimeData()->urls())     
          {
           fName = u.toLocalFile();
           info.setFile (fName);
           if (info.isFile())
               documents->open_file (fName);
           }
    */          
  event->acceptProposedAction();
}


void CApplication::saveState (QSessionManager &manager)
{
  manager.setRestartHint (QSessionManager::RestartIfRunning);
}


void CWavylon::nav_focus_to_fif()
{
  fif->setFocus (Qt::OtherFocusReason);
}



void CWavylon::help_show_gpl()
{
  show_text_file (":/COPYING");
}


void CWavylon::update_dyn_menus()
{
  update_palettes();
  opt_update_keyb();
  update_profiles();
  update_themes();

}


void CWavylon::view_toggle_fs()
{
  setWindowState (windowState() ^ Qt::WindowFullScreen);
}


void CWavylon::help_show_news()
{
  QString fname = ":/NEWS";
  if (QLocale::system().name().left(2) == "ru")
     fname = ":/NEWS-RU";

  show_text_file (fname);
}


void CWavylon::help_show_changelog()
{
  show_text_file (":/ChangeLog");
}


void CWavylon::help_show_todo()
{
  show_text_file (":/TODO");
}


void CAboutWindow::closeEvent (QCloseEvent *event)
{
  event->accept();
}


void CAboutWindow::update_image()
{
  QImage img (400, 128, QImage::Format_RGB32);

  QPainter painter (&img);

  for (int y = 1; y < 128 ; y += 16)
  for (int x = 1; x < 400; x += 25)
      {
       QColor color;
       
       int i = qrand() % 5;
       
       switch (i)
              {
               case 0: color = 0xfff3f9ff;
                       break;

               case 1: color = 0xffbfffb0;
                       break;

               case 2: color = 0xffa5a5a6;
                       break;
                       
               case 3: color = 0xffebffe9;
                       break;
               
               case 4: color = 0xffbbf6ff;
                       break;
              }
                  
       painter.fillRect (x, y, 25, 16, QBrush (color));
       
       if (i == 0) 
          {
           QFont f;
           f.setPixelSize (16);
           painter.setPen (Qt::gray);
           painter.setFont (f);
           painter.drawText (x, y + 16, "0");
          }

        if (i == 1) 
           {
            QFont f;
            f.setPixelSize (16);
            painter.setFont (f);
            painter.setPen (Qt::gray);
            painter.drawText (x, y + 16, "1");
           }
     } 

  QString txt = "Wavylon";
  
  QFont f ("Monospace");
  f.setPixelSize (85);
  painter.setFont (f);
  
  painter.setPen (Qt::black);
  painter.drawText (4, 90, txt);
  
  painter.setPen (Qt::red);
  painter.drawText (2, 86, txt);

  if (mascot_x > width() - mascot.width())
     mascot_right = false;
  if (mascot_x < 100)
     mascot_right = true;
     
  if (mascot_right)   
      mascot_x += 10;  
  else
      mascot_x -= 10;  
      
  painter.drawImage (mascot_x, 1, mascot);

  logo->setPixmap (QPixmap::fromImage (img));
}
  

CAboutWindow::CAboutWindow()
{
  setAttribute (Qt::WA_DeleteOnClose);

  mascot.load (":/icons/wavylon_icon.png");
  mascot_x = 100;
  mascot_right = true;

  QStringList sl_t = qstring_load (":/AUTHORS").split ("##");

  logo = new QLabel;
  update_image();
    
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(update_image()));
  timer->start (500);
  
  QTabWidget *tw = new QTabWidget (this);

  QPlainTextEdit *page_code = new QPlainTextEdit();
  QPlainTextEdit *page_thanks = new QPlainTextEdit();
  QPlainTextEdit *page_translators = new QPlainTextEdit();
  QPlainTextEdit *page_maintainers = new QPlainTextEdit();

  if (sl_t.size() == 4)
     {
      page_code->setPlainText (sl_t[0].trimmed());
      page_thanks->setPlainText (sl_t[3].trimmed());
      page_translators->setPlainText (sl_t[1].trimmed());
      page_maintainers->setPlainText (sl_t[2].trimmed());
     }

  tw->addTab (page_code, tr ("Code"));
  tw->addTab (page_thanks, tr ("Acknowledgements"));
  tw->addTab (page_translators, tr ("Translations"));
  tw->addTab (page_maintainers, tr ("Packages"));

  QVBoxLayout *layout = new QVBoxLayout();

  layout->addWidget(logo);
  layout->addWidget(tw);

  setLayout (layout);
  setWindowTitle (tr ("About"));
}


void CWavylon::cb_button_saves_as()
{
/*  CDocument *d = documents->get_current();
  if (! d)
     return;

  if (ed_fman_fname->text().isEmpty())
     return;

  QString filename (fman->dir.path());
  
  filename.append ("/").append (ed_fman_fname->text());

  if (file_exists (filename))
     if (QMessageBox::warning (this, "Wavylon",
                               tr ("%1 already exists\n"
                               "Do you want to overwrite?")
                               .arg (filename),
                               QMessageBox::Yes | QMessageBox::Default,
                               QMessageBox::Cancel | QMessageBox::Escape) == QMessageBox::Cancel)
         return;


   d->save_with_name (filename);
   
   QFileInfo f (d->file_name);
   dir_last = f.path();
   update_dyn_menus();
   fman->refresh();
   main_tab_widget->setCurrentIndex (idx_tab_edit);*/
}


void CWavylon::fman_home()
{
#ifdef Q_QS_WIN

  fman->nav ("c:/");

#else

  fman->nav (QDir::homePath());

#endif
}


void CWavylon::fman_add_bmk()
{
  sl_places_bmx.prepend (ed_fman_path->text());
  qstring_save (fname_places_bookmarks, sl_places_bmx.join ("\n"));
  update_places_bookmarks();
}


void CWavylon::fman_del_bmk()
{
  int i = lv_places->currentRow();

  if (i == -1)
     return; 

  QString s = lv_places->item(i)->text();
  if (s.isNull() || s.isEmpty())
     return;

  i = sl_places_bmx.indexOf (s);
  sl_places_bmx.removeAt (i);
  qstring_save (fname_places_bookmarks, sl_places_bmx.join ("\n"));
  update_places_bookmarks();
}


void CWavylon::fman_naventry_confirm()
{
  fman->nav (ed_fman_path->text());
}


void CWavylon::fman_places_itemActivated (QListWidgetItem *item)
{
  fman->nav (item->text());
}


void CWavylon::update_places_bookmarks()
{
  lv_places->clear();
  QStringList sl_items;
  
  if (! file_exists (fname_places_bookmarks))
     return;

  sl_places_bmx = qstring_load (fname_places_bookmarks).split ("\n");
  if (sl_places_bmx.size() != 0)
     lv_places->addItems (sl_places_bmx);
}


void CWavylon::fman_open()
{
  qDebug() << "CWavylon::fman_open()";

  main_tab_widget->setCurrentIndex (idx_tab_edit);
 
  QString f = ed_fman_fname->text().trimmed();
  QStringList li = fman->get_sel_fnames();
  
  qDebug() << "f1";

  if (! f.isNull() || ! f.isEmpty())
  if (f[0] == '/')
     {
      
      project_manager->project_open (f);
      
      if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);

           
      if (project_manager->project)
          dir_last = get_file_path (project_manager->project->paths.fname_project);


      main_tab_widget->setCurrentIndex (idx_tab_edit);
      return;
     }

  qDebug() << "f2";


  if (li.size() == 0)
     {
      QString fname (fman->dir.path());
      fname.append ("/").append (f);

      project_manager->project_open (fname);
      
      if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);

           
      if (project_manager->project)
          dir_last = get_file_path (project_manager->project->paths.fname_project);
           
      
      main_tab_widget->setCurrentIndex (idx_tab_edit);
      return;
     }

  qDebug() << "f3";

  foreach (QString fname, li)
          {
         project_manager->project_open (fname);
         
         if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);

            
         if (project_manager->project)
             dir_last = get_file_path (project_manager->project->paths.fname_project);
          
          }
}


void CWavylon::fman_create_dir()
{
  bool ok;
  QString newdir = QInputDialog::getText (this, tr ("Enter the name"),
                                                tr ("Name:"), QLineEdit::Normal,
                                                tr ("new_directory"), &ok);
  if (! ok || newdir.isEmpty())
     return;

  QString dname = fman->dir.path() + "/" + newdir;

  QDir d;
  if (! d.mkpath (dname))
     return;

  fman->nav (dname);
}


void CWavylon::view_show_mixer()
{
/*  bool visibility = ! wnd_fxrack->isVisible();
  wnd_fxrack->setVisible (visibility);
  settings->setValue ("wnd_fxrack_visible", visibility);*/
}


void CWavylon::view_stay_on_top()
{
  Qt::WindowFlags flags = windowFlags();
  flags ^= Qt::WindowStaysOnTopHint;
  setWindowFlags (flags );
  show();
  activateWindow();
}


void CWavylon::load_palette (const QString &fileName)
{
  if (! file_exists (fileName))
      return;

  global_palette.clear();
  global_palette = hash_load_keyval (fileName);
}



void CWavylon::file_use_palette()
{
  QAction *a = qobject_cast<QAction *>(sender());
  QString fname (dir_palettes);
  fname.append ("/").append (a->text());

  if (! file_exists (fname))
     fname = ":/palettes/" + a->text();

  fname_def_palette = fname;
  load_palette (fname);
  
  update_stylesheet (fname_stylesheet);
}


void CWavylon::update_palettes()
{
  menu_view_palettes->clear();

  QStringList l1 = read_dir_entries (dir_palettes);
  QStringList l2 = read_dir_entries (":/palettes");
  l1 += l2;

  create_menu_from_list (this, menu_view_palettes,
                         l1,
                         SLOT (file_use_palette()));
}


void CWavylon::fman_drives_changed (const QString & path)
{
  if (! ui_update)
     fman->nav (path);
}


void CWavylon::createFman()
{
  QWidget *wd_fman = new QWidget (this);

  QVBoxLayout *lav_main = new QVBoxLayout;
  QVBoxLayout *lah_controls = new QVBoxLayout;
  QHBoxLayout *lah_topbar = new QHBoxLayout;

  QLabel *l_t = new QLabel (tr ("Name"));
  ed_fman_fname = new QLineEdit;
  connect (ed_fman_fname, SIGNAL(returnPressed()), this, SLOT(fman_fname_entry_confirm()));

  
  ed_fman_path = new QLineEdit;
  connect (ed_fman_path, SIGNAL(returnPressed()), this, SLOT(fman_naventry_confirm()));

  tb_fman_dir = new QToolBar;
  tb_fman_dir->setObjectName ("tb_fman_dir");

  QAction *act_fman_go = new QAction (QIcon( ":/icons/go.png"), tr ("Go"), this);
  connect (act_fman_go, SIGNAL(triggered()), this, SLOT(fman_naventry_confirm()));

  QAction *act_fman_home = new QAction (QIcon (":/icons/home.png"), tr ("Home"), this);
  connect (act_fman_home, SIGNAL(triggered()), this, SLOT(fman_home()));

  QAction *act_fman_refresh = new QAction (QIcon (":/icons/refresh.png"), tr ("Refresh"), this);
  QAction *act_fman_ops = new QAction (QIcon (":/icons/create-dir.png"), tr ("Operations"), this);
  act_fman_ops->setMenu (menu_fm_file_ops);

  tb_fman_dir->addAction (act_fman_go);
  tb_fman_dir->addAction (act_fman_home);
  tb_fman_dir->addAction (act_fman_refresh);
  tb_fman_dir->addAction (act_fman_ops);

#if defined(Q_OS_WIN) || defined(Q_OS_OS2)

  cb_fman_drives = new QComboBox;
  lah_topbar->addWidget (cb_fman_drives);

  QFileInfoList l_drives = QDir::drives();
  foreach (QFileInfo fi, l_drives)
           cb_fman_drives->addItem (fi.path());
  
#endif

  lah_topbar->addWidget (ed_fman_path);
  lah_topbar->addWidget (tb_fman_dir);

  lah_controls->addWidget (l_t);
  lah_controls->addWidget (ed_fman_fname);

  
  QPushButton *bt_fman_open = new QPushButton (tr ("Open"), this);
  connect (bt_fman_open, SIGNAL(clicked()), this, SLOT(fman_open()));

  QPushButton *bt_fman_save_as = new QPushButton (tr ("Save as"), this);
  connect (bt_fman_save_as, SIGNAL(clicked()), this, SLOT(cb_button_saves_as()));

  lah_controls->addWidget (l_t);
    
    
  lah_controls->addWidget (bt_fman_open);
  lah_controls->addWidget (bt_fman_save_as);

  fman = new CFMan;

  connect (fman, SIGNAL(file_activated (const QString &)), this, SLOT(fman_file_activated (const QString &)));
  connect (fman, SIGNAL(dir_changed  (const QString &)), this, SLOT(fman_dir_changed  (const QString &)));
  connect (fman, SIGNAL(current_file_changed  (const QString &, const QString &)), this, SLOT(fman_current_file_changed  (const QString &, const QString &)));

  connect (act_fman_refresh, SIGNAL(triggered()), fman, SLOT(refresh()));

#if defined(Q_OS_WIN) || defined(Q_OS_OS2)

  connect (cb_fman_drives, SIGNAL(currentIndexChanged ( const QString & )),
          this, SLOT(fman_drives_changed(const QString & )));

#endif

  w_right = new QWidget (this);
  
  w_right->setMinimumWidth (10);

  
  QVBoxLayout *lw_right = new QVBoxLayout;
  w_right->setLayout (lw_right);  
  
  lw_right->addLayout (lah_controls);

  QFrame *vline = new QFrame;
  vline->setFrameStyle (QFrame::HLine);
  lw_right->addWidget (vline);

  QLabel *l_bookmarks = new QLabel (tr ("<b>Bookmarks</b>"));
  lw_right->addWidget (l_bookmarks);


  QHBoxLayout *lah_places_bar = new QHBoxLayout;
  QPushButton *bt_add_bmk = new QPushButton ("+");
  QPushButton *bt_del_bmk = new QPushButton ("-");
  lah_places_bar->addWidget (bt_add_bmk);
  lah_places_bar->addWidget (bt_del_bmk);

  connect (bt_add_bmk, SIGNAL(clicked()), this, SLOT(fman_add_bmk()));
  connect (bt_del_bmk, SIGNAL(clicked()), this, SLOT(fman_del_bmk()));

  lv_places = new QListWidget;
  update_places_bookmarks();
  connect (lv_places, SIGNAL(itemActivated (QListWidgetItem *)), this, SLOT(fman_places_itemActivated (QListWidgetItem *)));

  QVBoxLayout *vbox = new QVBoxLayout;
  vbox->addLayout (lah_places_bar);
  vbox->addWidget (lv_places);

  lw_right->addLayout (vbox);
  
  fman->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

  spl_fman = new QSplitter (this);
  spl_fman->setChildrenCollapsible (true);

  spl_fman->addWidget (fman);
  spl_fman->addWidget (w_right);
  
  spl_fman->restoreState (settings->value ("spl_fman").toByteArray());

  lav_main->addLayout (lah_topbar);
  lav_main->addWidget (spl_fman);

  wd_fman->setLayout (lav_main);

  fman_home();
  
  idx_tab_fman = main_tab_widget->addTab (wd_fman, tr ("manage"));
}


void CWavylon::fman_file_activated (const QString &full_path)
{
  main_tab_widget->setCurrentIndex (idx_tab_edit);

      
     project_manager->project_open (full_path);
           
      if (project_manager->project)
          dir_last = get_file_path (project_manager->project->paths.fname_project);

      if (project_manager->project)
         transportToolBar->addWidget (project_manager->project->transport_simple);


      main_tab_widget->setCurrentIndex (idx_tab_edit);
      return;
   
}


void CWavylon::fman_dir_changed (const QString &full_path)
{
  ui_update = true;
  ed_fman_path->setText (full_path);

#if defined(Q_OS_WIN) || defined(Q_OS_OS2)

  cb_fman_drives->setCurrentIndex (cb_fman_drives->findText (full_path.left(3).toUpper()));

#endif

  ui_update = false;
}


void CWavylon::fman_current_file_changed (const QString &full_path, const QString &just_name)
{
  ed_fman_fname->setText (just_name);
}


void CWavylon::fman_rename()
{
 QString fname = fman->get_sel_fname();
  if (fname.isNull() || fname.isEmpty())
     return;
  
  QFileInfo fi (fname);
  if (! fi.exists() && ! fi.isWritable())
     return;
  
  bool ok;
  QString newname = QInputDialog::getText (this, tr ("Enter the name"),
                                                 tr ("Name:"), QLineEdit::Normal,
                                                 tr ("new"), &ok);
  if (! ok || newname.isEmpty())
     return;
  
  QString newfpath (fi.path());
  newfpath.append ("/").append (newname);
  QFile::rename (fname, newfpath);
  update_dyn_menus();
  fman->refresh();

  QModelIndex index = fman->index_from_name (newname);
  fman->selectionModel()->setCurrentIndex (index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
  fman->scrollTo (index, QAbstractItemView::PositionAtCenter);
}


void CWavylon::fman_delete()
{
  QString fname = fman->get_sel_fname();
  if (fname.isNull() || fname.isEmpty())
     return;

  int i = fman->get_sel_index();

  QFileInfo fi (fname);
  if (! fi.exists() && ! fi.isWritable())
     return;
 
  if (QMessageBox::warning (this, "Wavylon",
                            tr ("Are you sure to delete\n"
                            "%1?").arg (fname),
                            QMessageBox::Yes | QMessageBox::Default,
                            QMessageBox::No | QMessageBox::Escape) == QMessageBox::No)
      return;
 
  QFile::remove (fname);
  update_dyn_menus();
  fman->refresh();

  if (i < fman->list.count())
     {
      QModelIndex index = fman->index_from_idx (i);
      fman->selectionModel()->setCurrentIndex (index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      fman->scrollTo (index, QAbstractItemView::PositionAtCenter);
     }
}


void CWavylon::fman_refresh()
{
  fman->refresh();
}



void CWavylon::handle_args()
{
  QStringList l = qApp->arguments();
  int size = l.size();
  if (size < 2)
     return;

  for (int i = 1; i < size; i++)
      {
       QString t = l.at(i);
          
       QFileInfo f (l.at(i));

       if (! f.isAbsolute())
          {
           QString fullname (QDir::currentPath());
           fullname.append ("/").append (l.at(i));
//           documents->open_file (fullname);
          }
       else
           ;//documents->open_file (l.at(i));
      }
}


void CWavylon::main_tab_page_changed (int index)
{
  if (idx_prev == idx_tab_tune)
     leaving_tune();

  idx_prev = index;

  if (index == idx_tab_fman)
     {
      fman->setFocus();
      fm_entry_mode = FM_ENTRY_MODE_NONE;
      idx_tab_fman_activate();
     }
  else       
  if (index == idx_tab_edit)
     idx_tab_edit_activate();
  else   
  if (index == idx_tab_tune)
     idx_tab_tune_activate();
  else   
  if (index == idx_tab_learn)
     idx_tab_learn_activate();
}


QString CWavylon::fif_get_text()
{
  QString t = fif->text();

  int i = sl_fif_history.indexOf (t);

  if (i != -1)
     {
      sl_fif_history.removeAt (i);
      sl_fif_history.prepend (t);
     }
  else
      sl_fif_history.prepend (t);

  if (sl_fif_history.count() > 77)
     sl_fif_history.removeLast();

  return t;
}


void CWavylon::fman_fname_entry_confirm()
{
  if (fm_entry_mode == FM_ENTRY_MODE_OPEN)
     fman_open();     
   
  if (fm_entry_mode == FM_ENTRY_MODE_SAVE)
     cb_button_saves_as();
}


void CWavylon::view_use_profile()
{
  QAction *a = qobject_cast<QAction *>(sender());
  QSettings s (a->data().toString(), QSettings::IniFormat);

  QPoint pos = s.value ("pos", QPoint (1, 200)).toPoint();
  QSize size = s.value ("size", QSize (600, 420)).toSize();
  mainSplitter->restoreState (s.value ("splitterSizes").toByteArray());
  resize (size);
  move (pos);
}


void CWavylon::profile_save_as()
{
  bool ok;
  QString name = QInputDialog::getText (this, tr ("Enter the name"),
                                              tr ("Name:"), QLineEdit::Normal,
                                              tr ("new_profile"), &ok);
  if (! ok || name.isEmpty())
     return;

  QString fname (dir_profiles);
  fname.append ("/").append (name);

  QSettings s (fname, QSettings::IniFormat);

  s.setValue ("pos", pos());
  s.setValue ("size", size());
  s.setValue ("splitterSizes", mainSplitter->saveState());

  s.sync();
  
  update_profiles();
}


void CWavylon::update_profiles()
{
  menu_view_profiles->clear();
  create_menu_from_dir (this,
                        menu_view_profiles,
                        dir_profiles,
                        SLOT (view_use_profile())
                       );
}


void CWavylon::fman_items_select_by_regexp (bool mode)
{
  QString ft = fif_get_text();
  if (ft.isEmpty()) 
      return;  
     
  l_fman_find = fman->mymodel->findItems (ft, Qt::MatchRegExp);
  
  if (l_fman_find.size() < 1)
     return; 
   
  QItemSelectionModel *m = fman->selectionModel();
  for (int i = 0; i < l_fman_find.size(); i++)
      if (mode) 
         m->select (fman->mymodel->indexFromItem (l_fman_find[i]), QItemSelectionModel::Select | QItemSelectionModel::Rows);
      else   
          m->select (fman->mymodel->indexFromItem (l_fman_find[i]), QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
}


void CWavylon::fman_select_by_regexp()
{
  fman_items_select_by_regexp (true);
}


void CWavylon::fman_deselect_by_regexp()
{
  fman_items_select_by_regexp (false);
}

  
void CWavylon::idx_tab_edit_activate()
{
//  fileMenu->menuAction()->setVisible (true);
  editMenu->menuAction()->setVisible (true);
  //menu_functions->menuAction()->setVisible (true);
  menu_search->menuAction()->setVisible (false);
  menu_nav->menuAction()->setVisible (true);
  menu_fm->menuAction()->setVisible (false);
  menu_view->menuAction()->setVisible (true);
  helpMenu->menuAction()->setVisible (true);
}


void CWavylon::idx_tab_tune_activate()
{
//  fileMenu->menuAction()->setVisible (true);
  editMenu->menuAction()->setVisible (false);
  //menu_functions->menuAction()->setVisible (true);
  menu_search->menuAction()->setVisible (false);
  menu_nav->menuAction()->setVisible (false);
  menu_fm->menuAction()->setVisible (false);
  menu_view->menuAction()->setVisible (true);
  helpMenu->menuAction()->setVisible (true);
}


void CWavylon::idx_tab_fman_activate()
{
 // fileMenu->menuAction()->setVisible (true);
  editMenu->menuAction()->setVisible (false);
  //menu_functions->menuAction()->setVisible (false);
  menu_search->menuAction()->setVisible (true);
  menu_nav->menuAction()->setVisible (false);
  menu_fm->menuAction()->setVisible (true);
  menu_view->menuAction()->setVisible (true);
  helpMenu->menuAction()->setVisible (true);
}


void CWavylon::idx_tab_learn_activate()
{
  //fileMenu->menuAction()->setVisible (true);
  editMenu->menuAction()->setVisible (false);
  //menu_functions->menuAction()->setVisible (false);
  menu_search->menuAction()->setVisible (true);
  menu_nav->menuAction()->setVisible (false);
  menu_fm->menuAction()->setVisible (false);
  menu_view->menuAction()->setVisible (true);
  helpMenu->menuAction()->setVisible (true);
}


void CWavylon::show_text_file (const QString &fname)
{
  text_file_browser->setPlainText (qstring_load (fname));
  text_file_browser->resize (width(), height());
  text_file_browser->show();
}


void CWavylon::show_html_data (const QString &data)
{
  text_file_browser->setHtml (data);
  text_file_browser->resize (width(), height());
  text_file_browser->show();
}



void CWavylon::ed_delete()
{
  }

/* 
void CWavylon::file_info()
{
 CDocument *d = documents->get_current(); 
  if (! d)
     return;
  
  int f = d->wave_edit->waveform->sound_buffer->sndfile_format & SF_FORMAT_TYPEMASK;
  QString format = file_formats->hformatnames.value (f);
  int st = d->wave_edit->waveform->sound_buffer->sndfile_format & SF_FORMAT_SUBMASK;
  QString subtype = file_formats->hsubtype.value (st);;
 
  log->log (tr ("samplerate: %1").arg (QString::number (d->wave_edit->waveform->sound_buffer->samplerate)));
  log->log (tr ("channels: %1").arg (QString::number (d->wave_edit->waveform->sound_buffer->channels)));
  log->log (subtype);
  log->log (format);

  log->log (d->file_name);
}
*/



void CWavylon::cb_show_meterbar_in_db_changed (int value)
{
  settings->setValue ("meterbar_show_db", value);
//  documents->apply_settings();
}



void CWavylon::ed_deselect()
{
}


void CWavylon::ed_select_all()
{
}


/*
void CWavylon::bt_set_def_format_clicked()
{
  int sndfile_format = 0;
  sndfile_format = sndfile_format | SF_FORMAT_WAV | SF_FORMAT_FLOAT;

  int t = settings->value ("def_sndfile_format", sndfile_format).toInt();

  CChangeFormatWindow *w = new CChangeFormatWindow (0, 0, t);

  w->channels->setValue (settings->value ("def_channels", 1).toInt());

  int i = 0;
  int sr = settings->value ("def_samplerate", 44100).toInt();
  
  i = w->cmb_samplerate->findText (QString::number (sr));
      
  if (i == -1)
     i = 0; 

  w->cmb_samplerate->setCurrentIndex (i);
  
  int result = w->exec();
  
  if (result != QDialog::Accepted)
     {
      delete w;
      return;
     }
    
  int f = file_formats->hformatnames.key (w->cmb_format->currentText());
  int stype = file_formats->hsubtype.key (w->cmb_subtype->currentText());
  
  int ff = 0;
  ff = f | stype; 
      
  settings->setValue ("def_sndfile_format", ff);
  settings->setValue ("def_samplerate", w->cmb_samplerate->currentText().toInt());
  settings->setValue ("def_channels", w->channels->value());

  delete w;
}
*/

void CWavylon::spb_def_channels_valueChanged (int i)
{
  settings->setValue ("def_channels", i);
}


void CWavylon::spb_max_undos_valueChanged (int i)
{
  settings->setValue ("max_undos", i);
}




void CWavylon::cb_play_looped_changed (int value)
{
}


void CWavylon::nav_play_looped()
{
//  cb_play_looped->setChecked (d->wave_edit->waveform->play_looped);
}


void CWavylon::spb_ogg_q_valueChanged (double d)
{
  settings->setValue ("ogg_q", d);
  ogg_q = d;
}



void CWavylon::cmb_icon_sizes_currentIndexChanged (const QString &text)
{
  settings->setValue ("icon_size", text);

  setIconSize (QSize (text.toInt(), text.toInt()));
  tb_fman_dir->setIconSize (QSize (text.toInt(), text.toInt()));

}


void CWavylon::update_stylesheet (const QString &f)
{

//Update paletted

  int darker_val = settings->value ("darker_val", 100).toInt();


  QFontInfo fi = QFontInfo (qApp->font());

  QString fontsize = "font-size:" + settings->value ("app_font_size", fi.pointSize()).toString() + "pt;";  
  QString fontfamily = "font-family:" + settings->value ("app_font_name", qApp->font().family()).toString() + ";";  
  QString edfontsize = "font-size:" + settings->value ("editor_font_size", "16").toString() + "pt;";  
  QString edfontfamily = "font-family:" + settings->value ("editor_font_name", "Serif").toString() + ";";  
 
  QString stylesheet; 
  
  stylesheet = "QWidget, QWidget * {" + fontfamily + fontsize + "}\n";
  
  stylesheet += "CLogMemo, QTextEdit, QPlainTextEdit, QPlainTextEdit * {" + edfontfamily + fontsize + "}\n";
  
  

//Update themed


  QString cssfile = qstring_load (f);

  QString css_path = get_file_path (f) + "/"; 

  cssfile = cssfile.replace ("./", css_path); 

  cssfile += stylesheet;


  qApp->setStyleSheet ("");
  qApp->setStyleSheet (cssfile);

}



void CWavylon::view_use_theme()
{
  QAction *a = qobject_cast<QAction *>(sender());
  QString css_fname = a->data().toString() + "/" + "stylesheet.css";
  
 // qDebug() << css_fname;
  
  if (! file_exists (css_fname))
     {
      log->log (tr ("There is no plugin file"));
      return;
    }

  update_stylesheet (css_fname);

  fname_stylesheet = css_fname;

  settings->setValue ("fname_stylesheet", fname_stylesheet);
      
}



bool has_css_file (const QString &path)
{
  QDir d (path);
  QStringList l = d.entryList();
  
  for (int i = 0; i < l.size(); i++)
     {
      if (l[i].endsWith(".css"))
          return true;
     }
 
  return false;
}

//uses dir name as menuitem, no recursion
void create_menu_from_themes (QObject *handler,
                               QMenu *menu,
                               const QString &dir,
                               const char *method
                               )
{
  menu->setTearOffEnabled (true);
  QDir d (dir);
  QFileInfoList lst_fi = d.entryInfoList (QDir::NoDotAndDotDot | QDir::Dirs,
                                          QDir::IgnoreCase | QDir::LocaleAware | QDir::Name);

  
  foreach (QFileInfo fi, lst_fi)
         {
          if (fi.isDir())
             {
              if (has_css_file (fi.absoluteFilePath()))   
                 {
                  QAction *act = new QAction (fi.fileName(), menu->parentWidget());
                  act->setData (fi.filePath());
                  handler->connect (act, SIGNAL(triggered()), handler, method);
                  menu->addAction (act);
                }
             else
                 {
                  QMenu *mni_temp = menu->addMenu (fi.fileName());
                  create_menu_from_themes (handler, mni_temp,
                                           fi.filePath(), method);

                 }
             }
         }
             
}


void CWavylon::update_themes()
{
  menu_view_themes->clear();
  create_menu_from_themes (this,
                            menu_view_themes,
                            ":/themes",
                            SLOT (view_use_theme())
                            );
 
  
  create_menu_from_themes (this,
                            menu_view_themes,
                            dir_themes,
                            SLOT (view_use_theme())
                            );
  
}



void CWavylon::cb_altmenu_stateChanged (int state)
{
  if (state == Qt::Unchecked)
      b_altmenu = false;
  else 
       b_altmenu = true;
  
  settings->setValue ("b_altmenu", b_altmenu);
}


MyProxyStyle::MyProxyStyle (QStyle * style): QProxyStyle (style)
{
     
}


void CWavylon::cmb_buffer_size_frames_currentIndexChanged (const QString &text)
{
  settings->setValue ("buffer_size_frames", text);
  buffer_size_frames = text.toInt();  
}
  


void CWavylon::fman_project_files()
{
  if (! project_manager->project)
     return;
  
   main_tab_widget->setCurrentIndex (idx_tab_fman);
   fm_entry_mode = FM_ENTRY_MODE_OPEN;

   fman->nav (project_manager->project->paths.wav_dir);
}


void CWavylon::project_call_wavs_wnd()
{
  if (! project_manager->project)
     return;
  
  project_manager->project->files_window_show();

}


void CWavylon::project_call_tracks_wnd()
{
  if (! project_manager->project)
     return;
  
  project_manager->project->tracks_window_show();

}


void CWavylon::prj_clip_props()
{
  if (! project_manager->project)
     return;

  project_manager->project->table_clip_props();
}   



void CWavylon::test()
{

  if (! project_manager->project)
     return;

//буфер микширования стемов в кадрах
  int a = buffer_size_frames_multiplier * buffer_size_frames;
  qDebug() << "a: " << a;
  
  qDebug() << frames_to_time_str (a, project_manager->project->settings.samplerate);
  

//  qDebug() << frames_to_time_str (500, 44100);
/*
 QWidget *w = QApplication::focusWidget();
 
 if (typeid(*w) == typeid(QTableWidget)) 
 {
  qDebug() << "ok";
}*/

  //int index = project_manager->project->table_widget_layout->indexOf

}


  
void CWavylon::track_create_new()
{
  qDebug() << "CWavylon::track_create_new()";

  if (! project_manager->project)
     return;

  project_manager->project->track_insert_new();
  
//  project_manager->project->update_table();
}
 
void CWavylon::track_delete()
{
  if (! project_manager->project)
     return;


}
 
 
void CWavylon::track_up()
{
  if (! project_manager->project)
     return;

}


void CWavylon::track_down()
{
  if (! project_manager->project)
     return;

}


void CWavylon::project_props()
{
  if (! project_manager->project)
     return;
     
  project_manager->project->settings.call_ui();   

}


void CWavylon::project_mixdown()
{
 if (! project_manager->project)
     return;
     
  project_manager->project->mixdown_to_default();   


}
