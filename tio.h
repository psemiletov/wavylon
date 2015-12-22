#ifndef TIO_H
#define TIO_H

#include <QObject>
#include <QMultiHash>
#include <QHash>
#include <QStringList>


#include <sndfile.h>

//#ifdef MAD_ENABLED
//#include <mad.h>
//#endif


class CFileFormats
{
public:

  QMultiHash <int, int> hformat;
  QHash <int, QString> hsubtype;
  QHash <int, QString> hformatnames;
  QHash <int, QString> hextensions;

  CFileFormats();
};


class CSignaturesList: public QObject
{
  Q_OBJECT

public:
  QString encname;
  QList <QByteArray> words;
};


class CTio: public QObject
{
  Q_OBJECT

public:

  QString id;

  bool ronly;
  
  size_t total_samples;
  
  float *input_data;

  size_t frames;
  
  int samplerate;
  int channels;
  int format;
  
  QStringList extensions;
  QString error_string;

  virtual float* load (const QString &fname) = 0;
  virtual bool save (const QString &fname) = 0;
  virtual bool save_16bit_pcm (const QString &fname) = 0;
};


class CTioPlainAudio: public CTio
{
  Q_OBJECT

public:

  CTioPlainAudio();
  CTioPlainAudio (bool rnly);

  ~CTioPlainAudio();
  float* load (const QString &fname);
  bool save (const QString &fname);
  bool save_16bit_pcm (const QString &fname);
};

/*
#ifdef MAD_ENABLED
class CTioMad: public CTio
{
  Q_OBJECT

public:

  CTioMad();
  ~CTioMad();
  float* load (const QString &fname);
  bool save (const QString &fname);
  bool save_16bit_pcm (const QString &fname);
};
#endif
*/


class CTioLAME: public CTio
{
  Q_OBJECT

public:

  CTioLAME();
  ~CTioLAME();
  float* load (const QString &fname);
  bool save (const QString &fname);
  bool save_16bit_pcm (const QString &fname);
};


class CTioProxy: public CTio
{
  Q_OBJECT

public:

  QHash <QString, QString> proxies;

  CTioProxy();
  ~CTioProxy();
  float* load (const QString &fname);
  bool save (const QString &fname);
  bool save_16bit_pcm (const QString &fname);
};



class CTioReadOnly: public CTio
{
  Q_OBJECT

public:

  bool save (const QString &fname);
};


class CTioHandler: public QObject
{
  Q_OBJECT

public:

  QList <CTio *> list;
  
  QStringList supported_extensions;
  
  CTioHandler();
  ~CTioHandler();

  bool is_ext_supported (const QString &fname);

  CTio* get_for_fname (const QString &fname);
};


void file_formats_init();
void file_formats_done();

float* load_from_lame (QString &fname);


#endif // TIO_H
