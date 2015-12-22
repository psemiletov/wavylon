#ifndef LEVELMETER_H
#define LEVELMETER_H

#include <QWidget>
#include <QImage>

class CLevelMeter: public QWidget
{
  Q_OBJECT

public:
  
  bool init_state;
  bool muted;

  QImage img_bar;
 
  QColor color_l;
  QColor color_r;
 
  int scale_width;
  int bars_width;
  
  float peak_l;
  float peak_r;
  
  float pl;
  float pr;

  CLevelMeter (int scale_w = 32, int bars_w = 16, int h = 192, const QColor &cl = Qt::green, const QColor &cr = Qt::darkGreen, QWidget *parent = 0);

  void update_scale_image();

protected:

 void paintEvent (QPaintEvent *event);
 //void resizeEvent(QResizeEvent *event);
};


#endif // LEVELMETER_H
