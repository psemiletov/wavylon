#ifndef TINYPLAYER_H
#define TINYPLAYER_H

#include <QString>


class CTinyPlayer: public QObject
{
  Q_OBJECT

public:

  QString filename;

  bool load (const QString &fname);
  
  void play();
  void stop();
  void pause();
  
  CTinyPlayer();
  ~CTinyPlayer();

};



#endif // TINYPLAYER_H
