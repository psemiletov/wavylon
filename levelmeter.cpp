#include <QPainter>
#include <QFont>
#include <QResizeEvent>
#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QDebug>


#include "db.h"

#include "utils.h"
#include "levelmeter.h"


void CLevelMeter::update_scale_image()
{
  QImage im (scale_width, height(), QImage::Format_RGB32);

  QPainter painter (&im);

  painter.fillRect (0, 0, scale_width, height(), Qt::white);

  painter.setPen (Qt::black);
  
  QFont fnt ("Mono");
  fnt.setPixelSize (8);
  painter.setFont (fnt);
  
  int channel_height = height();  
 
  int db = -27;
    
   while (db <= 0)
       {
        float linval = db2lin (db);
      
        int ymin = channel_height - linval * channel_height;
        
        int y = abs(ymin);  
        
      //  qDebug() << "y " << y;

        QPoint p1 (1, y);
        QPoint p2 (5, y);
        painter.drawLine (p1, p2);
       
        painter.drawText (QPoint (1, y), QString::number (db, 'g', 2)); //dB
         
        if (db > -9)
            db++;
        else
            db += 5;
       }            
     
  img_bar = im;
}


/*
void CLevelMeter::update_scale_image()
{
  QImage im (scale_width, height(), QImage::Format_RGB32);

  QPainter painter (&im);

  painter.fillRect (0, 0, scale_width, height(), Qt::white);

  painter.setPen (Qt::black);
  
  QFont fnt ("Mono");
  fnt.setPixelSize (8);
  painter.setFont (fnt);
 
  int ten = get_value (height(), 5);  
                    
  int percentage = 105;
  float ff = 0.0f;
  
  for (int y = 0; y < height(); y++)
      {
       if (y % ten == 0)
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
*/

/*
void CLevelMeter::resizeEvent (QResizeEvent *event)
{
  //update_scale_image();
}
*/

CLevelMeter::CLevelMeter (int scale_w, int bars_w, int h, const QColor &cl, const QColor &cr, QWidget *parent): QWidget (parent)
{
  peak_l = 0;
  peak_r = 0;

  color_l = cl;
  color_r = cr;
  
  muted = false;

  scale_width = scale_w;
  bars_width = bars_w;

  setMinimumWidth (scale_width + bars_width);
  setMaximumWidth (scale_width + bars_width);

  setMinimumHeight (h);
  setMaximumHeight (h);

  resize (scale_width + bars_width, h);
  
  update_scale_image();

}


#define FALLOFF_COEF 0.05f

//#define FALLOFF_COEF 0.07f


void CLevelMeter::paintEvent (QPaintEvent *event)
{

//Первый фоллоф после обновления пика надо ПРОПУСКАТЬ! напиши код!

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

  painter.drawImage (1, 1, img_bar);
 
  if (muted)
     {
      event->accept();
      return; 
     }
       
  int lenl = h - (int)(peak_l * 1.0f * h);
  int lenr = h - (int)(peak_r * 1.0f * h);
      
  QPoint ltop (scale_width, height());
  QPoint lbottom (scale_width + (width() - scale_width) / 2, lenl);
  
  QPoint rtop (scale_width + (width() - scale_width) / 2, height());
  QPoint rbottom (width(), lenr);
  
  QRect l (ltop, lbottom);
  QRect r (rtop, rbottom);
    
  painter.fillRect (l, color_l);
  painter.fillRect (r, color_r);

  event->accept();
}
