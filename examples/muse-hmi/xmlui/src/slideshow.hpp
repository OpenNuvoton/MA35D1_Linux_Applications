// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "model.hpp"
#include "raster_cache.hpp"
#include <QImage>
#include <QFile>
#include <QPainter>
#include <QSize>
#include <cmath>
// Read the existing Photos tab cache independently of the native video player.
// Retain only two panel-sized frames; a slideshow never claims the audio device.
class PhotoSlideshow {
 QString folder=QString::fromLocal8Bit(qgetenv("MUSE_MEDIA_CACHE_DIR"));
 J album=J::object();std::string signature;double polled=-10;QSize dimensions;
public:
 QImage current,previous;QString title="Photos",caption;int index=-1,count=0;double changed=-1;
 PhotoSlideshow(){if(folder.isEmpty())folder="/mnt/sd/muse/media-cache";}
 void update(double seconds,QSize size){
  if(seconds-polled>=2){polled=seconds;QFile file(folder+"/tabs.json");if(file.open(QIODevice::ReadOnly)&&file.size()<=1048576){try{auto tabs=J::parse(file.readAll().constData());if(tabs.is_array()&&tabs.size()==5&&tabs[1].is_object()){auto next=tabs[1];auto key=next.dump();if(key!=signature){album=next;signature=key;index=-1;}}}catch(...){}}}
  count=album.contains("slides")&&album["slides"].is_array()?std::min(40,int(album["slides"].size())):album.contains("url")?1:0;
  if(!count)return;
  int interval=std::clamp(album.value("interval_seconds",5),1,60),next=int(std::floor(seconds/interval))%count;if(next==index&&dimensions==size)return;
  auto base=QString::fromStdString(album.value("_cache_file",std::string()));if(!base.startsWith(folder+"/"))return;auto cache=base+(album.contains("slides")?"-"+QString::number(next+1):"");muse_media::RasterCache pixels;QImage image;
  if(pixels.open((cache+".rgb").toStdString()))image=QImage(pixels.pixels(),pixels.width,pixels.height,pixels.width*3,QImage::Format_RGB888).copy();
  if(image.isNull()){int w=0,h=0,n=0;auto path=cache.toLocal8Bit();if(stbi_info(path.constData(),&w,&h,&n)&&w>0&&h>0&&w<=4096&&h<=4096&&int64_t(w)*h<=4000000){auto bytes=stbi_load(path.constData(),&w,&h,&n,4);if(bytes){image=QImage(bytes,w,h,w*4,QImage::Format_RGBA8888).copy();stbi_image_free(bytes);}}}
  if(image.isNull())return;
  image=image.scaled(size,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);int x=(image.width()-size.width())/2,y=(image.height()-size.height())/2;
  previous=current;current=image.copy(x,y,size.width(),size.height()).convertToFormat(QImage::Format_RGB32);changed=seconds;index=next;dimensions=size;
  title=QString::fromStdString(album.value("caption",std::string("Photos")));caption=album.contains("slides")?QString::fromStdString(album["slides"][next].value("caption",std::string())):QString();
 }
};
