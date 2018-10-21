 /**************************************************************************
 *   2010-2015 by Peter Semiletov                                          *
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


#ifndef WAVYLON_H
#define WAVYLON_H

#include <QObject>
#include <QApplication>
#include <QSessionManager>
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTranslator>
#include <QProgressBar>
#include <QListWidgetItem>
#include <QSplitter>
#include <QTextBrowser>
#include <QFontComboBox>
#include <QSettings>
#include <QMainWindow>

#include "project.h"

#include "logmemo.h"
#include "fman.h"
#include "shortcuts.h"


/*
class CChangeFormatWindow: public QDialog
{
  Q_OBJECT

public:

  int fmt;
  CWaveform *wf;
  
  QLabel *file_name;
  
  QComboBox *cmb_format;
  QComboBox *cmb_subtype;
  QComboBox *cmb_samplerate;
  
  QSpinBox *channels;
  
  CChangeFormatWindow (QWidget *parent, CWaveform *waveform, int fm);

public slots:

  void format_currentIndexChanged (const QString &text);
};
*/

class CAboutWindow: public QWidget
{
Q_OBJECT

  QLabel *logo;
  
public:

  int mascot_x;
  bool mascot_right;

  QImage mascot;
  CAboutWindow();

protected:

  void closeEvent (QCloseEvent *event);
  
public slots:

  void update_image();
  
};

  

class CWavylon: public QMainWindow
{
  Q_OBJECT

public:

  CProjectManager *project_manager;

  CWavylon();
  ~CWavylon();
  
  int fm_entry_mode; 

  CShortcuts *shortcuts;
  CFMan *fman;

  CLogMemo *log;

    
  QString fname_stylesheet; 

  bool b_preview;

  int idx_tab_timeline;
  int idx_tab_edit;
  int idx_tab_tune;
  int idx_tab_fman;
  int idx_tab_learn;
  int idx_prev;
  
  int fman_find_idx;
  QList <QStandardItem *> l_fman_find;
  
  bool ui_update;

  QLabel *l_maintime;

  QToolBar *tb_fman_dir;
  QLabel *l_fman_preview;
 
  QStringList sl_places_bmx;
  QStringList sl_urls;
  QStringList sl_fif_history;

  QHash <QString, QString> programs;
  QHash <QString, QString> places_bookmarks;

  QTranslator myappTranslator;
  QTranslator qtTranslator;

  QDir dir_lv;

  QComboBox *cmb_fif;
  QCheckBox *cb_play_looped;
 
  QComboBox *cmb_panner;
  
 
  QString man_search_value;

  QString opt_shortcuts_string_to_find;
  QString fman_fname_to_find;

  QString dir_profiles;  
  QString dir_last;
  QString dir_config;
  QString dir_sessions;

  QString dir_scripts;
  QString dir_palettes;
  
  QString fname_def_palette;

  QString fname_fif;

  QString fname_places_bookmarks;
  QString dir_themes;



  QString fname_tempparamfile;

  QLabel *l_status;
  QProgressBar *pb_status;



  QCheckBox *cb_altmenu;
  
  QLineEdit *ed_temp_path;
  QLineEdit *ed_mp3_encode;


  QSpinBox *sp_buffer_size_frames_multiplier;

  QComboBox *cmb_src;
  QDoubleSpinBox *spb_ogg_q;

  QComboBox *cmb_sound_dev_out;
  QComboBox *cmb_sound_dev_in;

  QMenu *menu_view_themes;


protected:

  void closeEvent (QCloseEvent *event);
  bool fman_tv_eventFilter (QObject *obj, QEvent *event);


public slots:

/*************************
main window callbacks
*************************/

  void leaving_tune();

  
  void project_mixdown();

  
  void nav_play_looped();

  void view_show_mixer();
  
  void cb_play_looped_changed (int value);

  void cb_show_meterbar_in_db_changed (int value);
  void spb_max_undos_valueChanged (int i);

//  void slot_transport_play();
//  void slot_transport_stop();

  void cmb_icon_sizes_currentIndexChanged (const QString &text);

  void cmb_sound_dev_out_currentIndexChanged (int index);
  void cmb_sound_dev_in_currentIndexChanged (int index);
  void cmb_mono_recording_mode_currentIndexChanged (int index);

  void cb_monitor_input_changed (int state);

  void cmb_panner_currentIndexChanged (int index);


 // void stop_recording();
  
  //void fm_full_info();

  void prj_clip_props();
  void prj_clip_del();
  void prj_clip_repeat_cloned();


  
  void fman_refresh();
  void fman_rename();
  void fman_delete();
  void fman_drives_changed (const QString & path);
  void fman_current_file_changed (const QString &full_path, const QString &just_name);
  void fman_file_activated (const QString &full_path);
  void fman_dir_changed (const QString &full_path);
  void fman_fname_entry_confirm();
  void fman_create_dir();
  void fman_add_bmk();
  void fman_del_bmk();
  void fman_open();
  void fman_home();
  void fman_places_itemActivated (QListWidgetItem *item);
  void fman_select_by_regexp();
  void fman_deselect_by_regexp();

  void cb_button_saves_as();

  void pageChanged (int index);


  //void file_record();
  
  void newFile();
  void open();
  
  void fman_items_select_by_regexp (bool mode);

//  void file_reload();
  
  //void file_export_mp3();
  
  
  void file_last_opened();
  void file_use_palette();
  //void file_open_session();
  void file_save_version();
  //void file_change_format();
 

  void cb_altmenu_stateChanged (int state);

  void test();

  void view_use_profile();

  bool save();
  bool saveAs();

  void about();
  void close_current();

  void ed_copy();
  
  void ed_delete();

  void ed_paste();
  void ed_cut();

  void ed_undo();
  void ed_redo();
  
  void ed_deselect();
  void ed_select_all();
  //void ed_trim();
 

  void search_find();
  void search_find_next();
  void search_find_prev();
  
  void view_toggle_fs();
  void view_stay_on_top();

  void nav_focus_to_fif();
 // void nav_focus_to_editor();

  void help_show_gpl();
  void help_show_news();
  void help_show_changelog();
  void help_show_todo();

//  void session_save_as();

  void main_tab_page_changed (int index);
  void profile_save_as();

//  void file_info();

  void man_find_find();
  void man_find_next();
  void man_find_prev();


  void fman_project_files();


/*************************
prefs window callbacks
*************************/



  //void cmb_src_currentIndexChanged (int index);

 

  void spb_def_channels_valueChanged (int i);
  
 // void bt_set_def_format_clicked();
  void pb_choose_temp_path_clicked();

  void cb_locale_override (int state);
  void ed_locale_override_editingFinished();
  void cb_session_restore (int state);

  void cb_use_trad_dialogs_changed (int state);

  void cmb_proxy_video_decoder_currentIndexChanged (int index);


  void pb_assign_hotkey_clicked();
  void pb_remove_hotkey_clicked();
  
  void slot_lv_menuitems_currentItemChanged (QListWidgetItem *current, QListWidgetItem *previous);
  void slot_app_fontname_changed (const QString &text);
  
  void slot_app_font_size_changed (int i);
  void slot_style_currentIndexChanged (const QString &text);
  void slot_sl_icons_size_sliderMoved (int value);

  void cmb_buffer_size_frames_currentIndexChanged (const QString &text);
        

  void spb_ogg_q_valueChanged (double d);

  void fman_naventry_confirm();

  void view_use_theme();

  void project_call_tracks_wnd();
  
  void project_call_wavs_wnd();
  void project_props();

    
  void track_create_new();
  void track_delete();
  void track_up();
  void track_down();

  
  void fman_find();
  void fman_find_next();
  void fman_find_prev();
 
  void opt_update_keyb();
  void opt_shortcuts_find();
  void opt_shortcuts_find_next();
  void opt_shortcuts_find_prev();

  void idx_tab_edit_activate();
  void idx_tab_tune_activate();
  void idx_tab_fman_activate();
  void idx_tab_learn_activate();


private:

/*************************
main window widgets
*************************/

  QSplitter *mainSplitter;
  QTextBrowser *man;
  QPlainTextEdit *log_memo;
  QString charset;

  QTabWidget *main_tab_widget;
  QTabWidget *tab_options;
  QTabWidget *tab_browser;
  
 QTabWidget *tab_widget;
  
  QLineEdit *fif;

 // QMenu *fileMenu;
  QMenu *editMenu;

  QMenu *menu_file_configs;
//  QMenu *menu_file_sessions;
  QMenu *menu_file_actions;
  
  QMenu *menu_view_palettes;
  QMenu *menu_view_profiles;

//  QMenu *menu_fn_sessions;
  
//  QMenu *menu_functions;
  QMenu *menu_project;
  QMenu *menu_track;
  QMenu *menu_clip;
  
  QMenu *menu_fn_channels;
  //QMenu *menu_fn_amp;
  
  
  QMenu *menu_file_recent;
  QMenu *menu_search;
  
  QMenu *menu_nav;
  QMenu *menu_fm;
  QMenu *menu_fm_file_ops;
  QMenu *menu_fm_file_infos;
  QMenu *menu_view;
  
  QMenu *helpMenu;

  QToolBar *fileToolBar;
  QToolBar *editToolBar;
  QToolBar *transportToolBar;
  QToolBar *levelMeterToolBar;


  QAction *act_test;
  QAction *newAct;
  QAction *openAct;
  QAction *saveAct;
  QAction *saveAsAct;
  QAction *exitAct;
  QAction *cutAct;
  QAction *copyAct;
  QAction *closeAct;
  QAction *undoAct;
  QAction *redoAct;
  QAction *pasteAct;
  QAction *aboutAct;
  QAction *aboutQtAct;

  QAction *transport_play;
  QAction *transport_stop;
  
  
  QWidget *w_right;

  QLineEdit *ed_fman_fname;
  QComboBox *cb_fman_drives;


/*************************
prefs window widgets
*************************/


  QLineEdit *ed_locale_override;

  CShortcutEntry *ent_shtcut;
  QListWidget *lv_menuitems;


  QComboBox *cmb_buffer_size_frames;
  
  QFontComboBox *cmb_app_font_name;
  QSpinBox *spb_app_font_size;

  QLineEdit *ed_fman_path;
  QListWidget *lv_places;
  QSplitter *spl_fman;

  QTextBrowser *text_file_browser;




  QAction* add_to_menu (QMenu *menu,
                        const QString &caption,
                        const char *method,
                        const QString &shortkt = QString(),
                        const QString &iconpath = QString()
                        );


  void update_dyn_menus();
  void create_paths();
  
  void init_styles();


  void handle_args();

  void update_themes();
  void update_stylesheet (const QString &f);

  void load_palette (const QString &fileName);
  
  

  void create_main_widget();
  void createActions();
  void createMenus();
  void createOptions();

  void createManual();
  void updateFonts();
  void update_sessions();
  void update_palettes();

  void update_places_bookmarks();

  void update_profiles();

  void createToolBars();
  void createStatusBar();
  void readSettings();
  void writeSettings();

  void dragEnterEvent (QDragEnterEvent *event);
  void dropEvent (QDropEvent *event);

  void createFman();


  QString fif_get_text();

  

  void show_text_file (const QString &fname);
  void show_html_data (const QString &data);
    
  void fn_ch_mono_to_stereo (bool full); 

};


class CApplication: public QApplication
{
  Q_OBJECT

public:

  CApplication (int &argc, char **argv): QApplication (argc, argv)
               {}

  void saveState (QSessionManager &manager);
};


#endif
