#include "dialog_cam.h"
#include "ui_dialog_cam.h"
#include "mainwindow.h"
#include <time.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>

#include "mtcnn/detector.h"
#include "draw.hpp"

#define FRAME_DETECTION_X (160)
#define FRAME_DETECTION_Y (120)

#define PEOPLECNT_RESOLUTION (FRAME_DETECTION_X*FRAME_DETECTION_Y)
#define NORMAL_RESOLUTION ((FRAME_DETECTION_X*4)*(FRAME_DETECTION_Y*4))

using namespace std;
using namespace cv;

std::string modelPath = "/opt/models";

ProposalNetwork::Config pConfig;
RefineNetwork::Config rConfig;
OutputNetwork::Config oConfig;

//##### QImage ---> cv::Mat #####

cv::Mat QImageToCvMat( const QImage &inImage, bool inCloneImageData = true )
{
	switch ( inImage.format() )
	{
		// 8-bit, 4 channel
		case QImage::Format_ARGB32:
		case QImage::Format_ARGB32_Premultiplied:
			{
				cv::Mat  mat( inImage.height(), inImage.width(),
						CV_8UC4,
						const_cast<uchar*>(inImage.bits()),
						static_cast<size_t>(inImage.bytesPerLine())
					    );

				return (inCloneImageData ? mat.clone() : mat);
			}

			// 8-bit, 3 channel
		case QImage::Format_RGB32:
		case QImage::Format_RGB888:
			{
				if ( !inCloneImageData )
				{
					qWarning() << "CVS::QImageToCvMat() - Conversion requires cloning because we use a temporary QImage";
				}

				QImage   swapped = inImage;

				if ( inImage.format() == QImage::Format_RGB32 )
				{
					swapped = swapped.convertToFormat( QImage::Format_RGB888 );
				}

				swapped = swapped.rgbSwapped();

				return cv::Mat( swapped.height(), swapped.width(),
						CV_8UC3,
						const_cast<uchar*>(swapped.bits()),
						static_cast<size_t>(swapped.bytesPerLine())
					      ).clone();
			}

			// 8-bit, 1 channel
		case QImage::Format_Indexed8:
			{
				cv::Mat  mat( inImage.height(), inImage.width(),
						CV_8UC1,
						const_cast<uchar*>(inImage.bits()),
						static_cast<size_t>(inImage.bytesPerLine())
					    );

				return (inCloneImageData ? mat.clone() : mat);
			}

		default:
			qWarning() << "CVS::QImageToCvMat() - QImage format not handled in switch:" << inImage.format();
			break;
	}

	return cv::Mat();
}
cv::Mat QPixmapToCvMat( const QPixmap &inPixmap, bool inCloneImageData = true )
{
	return QImageToCvMat( inPixmap.toImage(), inCloneImageData );
}



//##### cv::Mat ---> QImage #####
inline QImage  cvMatToQImage( const cv::Mat &inMat )
{
	switch ( inMat.type() )
	{
		// 8-bit, 4 channel
		case CV_8UC4:
			{
				QImage image( inMat.data,
						inMat.cols, inMat.rows,
						static_cast<int>(inMat.step),
						QImage::Format_ARGB32 );

				return image;
			}

			// 8-bit, 3 channel
		case CV_8UC3:
			{
				QImage image( inMat.data,
						inMat.cols, inMat.rows,
						static_cast<int>(inMat.step),
                        QImage::Format_RGB888);

				return image.rgbSwapped();
			}

			// 8-bit, 1 channel
		case CV_8UC1:
			{
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
				QImage image( inMat.data,
						inMat.cols, inMat.rows,
						static_cast<int>(inMat.step),
						QImage::Format_Grayscale8 );//Format_Alpha8 and Format_Grayscale8 were added in Qt 5.5
#else//
				static QVector<QRgb>  sColorTable;

				// only create our color table the first time
				if ( sColorTable.isEmpty() )
				{
					sColorTable.resize( 256 );

					for ( int i = 0; i < 256; ++i )
					{
						sColorTable[i] = qRgb( i, i, i );
					}
				}

				QImage image( inMat.data,
						inMat.cols, inMat.rows,
						static_cast<int>(inMat.step),
						QImage::Format_Indexed8 );

				image.setColorTable( sColorTable );
#endif

				return image;
			}

		default:
			qWarning() << "CVS::cvMatToQImage() - cv::Mat image type not handled in switch:" << inMat.type();
			break;
	}

	return QImage();
}

//##### cv::Mat ---> QPixmap #####
inline QPixmap cvMatToQPixmap( const cv::Mat &inMat )
{
	return QPixmap::fromImage( cvMatToQImage( inMat ) );
}

Dialog_Cam::Dialog_Cam(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::Dialog_Cam)
{
    ui->setupUi(this);
    b_peoplecnt_en = false;
    b_facedetect_done = true;
    pWnd_main->b_dlg_cam_on = true;
	timer_people_cnt = new QTimer(this);
	pWnd_cam = this;
	thread_people_cnt = new People_Cnt;
	init_Cam();

	pConfig.caffeModel = modelPath + "/det1.caffemodel";
	pConfig.protoText = modelPath + "/det1.prototxt";
	pConfig.threshold = 0.6f;

	rConfig.caffeModel = modelPath + "/det2.caffemodel";
	rConfig.protoText = modelPath + "/det2.prototxt";
	rConfig.threshold = 0.7f;

	oConfig.caffeModel = modelPath + "/det3.caffemodel";
	oConfig.protoText = modelPath + "/det3.prototxt";
	oConfig.threshold = 0.7f;

}

Dialog_Cam::~Dialog_Cam()
{
    pWnd_main->b_dlg_cam_on = false;
	if(camera){
		camera->unload();
		camera->stop();
		delete  camera;
	}
	if(viewfinder)          delete  viewfinder;
	if(timer_people_cnt)    delete  timer_people_cnt;
	if(b_peoplecnt_en){
		thread_people_cnt->stop();
		while(thread_people_cnt->isRunning()){};
		delete  thread_people_cnt;
	}
	delete  ui;
}

void Dialog_Cam::on_pushButton_close_clicked()
{
	delete this;
}

int Dialog_Cam::init_Cam(){
	QComboBox *cameraType = new QComboBox();
	cameraType = ui->comboBox_cam_select;
	cameraType->clear();
	box = ui->comboBox_resolution;
	cameras = QCameraInfo::availableCameras();

	foreach(const QCameraInfo &cameraInfo, cameras) {
		qDebug() << "CameraInfo:" << cameraInfo;
		cameraType->addItem(cameraInfo.description());
	}

	viewfinder = new SharedViewfinder;
	connect(viewfinder, SIGNAL(frameReady(QPixmap)), this, SLOT(frameReady(QPixmap)));

	camera = new QCamera(this);
	camera->setCaptureMode(QCamera::CaptureVideo);
	camera->setViewfinder(viewfinder);

	QTimer::singleShot(500, this, SLOT(setfblComobox()));
	camera->load();
	return 0;
}

int Dialog_Cam::open_Cam(){
	return 0;
}

int Dialog_Cam::close_Cam(){
	if(camera){
		camera->stop();
		delete camera;
	}
    if(viewfinder)
        delete viewfinder;

	return 0;
}

void Dialog_Cam::on_comboBox_cam_select_activated(int index)
{
	printResolutionSize();
	index = ui->comboBox_cam_select->currentIndex();
	qDebug()<<"Index"<< index <<": "<< ui->comboBox_cam_select->currentText();
	camera->stop();
	delete camera;
	camera = new QCamera(cameras[index]);
	camera->setCaptureMode(QCamera::CaptureVideo);
	camera->setViewfinder(viewfinder);
	QTimer::singleShot(500, this, SLOT(setfblComobox()));
	camera->load();
}

inline int Dialog_Cam::find_resolution_idx(int resolution){
    int ret = -1;
    foreach (QSize msize, mResSize) {
        if(msize.width()*msize.height() <= resolution)
            ret++;
    }
    return ret;
}

void Dialog_Cam::setfblComobox(){
	printResolutionSize();
	mResSize.clear();
	mResSize = camera->supportedViewfinderResolutions();

	box->clear();
	int i=0;
	foreach (QSize msize, mResSize) {
		qDebug()<<msize;
		box->addItem(QString::number(msize.width(),10)+"*"+QString::number(msize.height(),10), i++);
	}

	if(mResSize.size() == 0)
		return;

    int idx = find_resolution_idx(NORMAL_RESOLUTION);
    idx_resolution_display = idx;
    if(idx<0){
        qDebug()<<"idx error";
        return;
    }
	box->setCurrentIndex(idx);
	QCameraViewfinderSettings set;
	set.setResolution(mResSize[idx]);
	camera->setViewfinderSettings(set);
	camera->start();
}
void Dialog_Cam::on_comboBox_resolution_activated(int index)
{
	camera->stop();
	QCameraViewfinderSettings set;

	printResolutionSize();
	index = ui->comboBox_resolution->currentIndex();
	qDebug()<<"Index"<< index <<": "<< ui->comboBox_resolution->currentText();
	qDebug()<<"mResSize:"<<mResSize[index];

	set.setResolution(mResSize[index]);

    camera->setViewfinderSettings(set);
    camera->start();
}

void Dialog_Cam::printResolutionSize(){
	QList<QSize> sizes = camera->supportedViewfinderResolutions();
	qDebug() << "viewfinderResolutions sizes.len = " << sizes.length();
	foreach (QSize size, sizes) {
		qDebug() << "Resolutions size = " << size;
	}
}

bool Dialog_Cam::GetFaceDetectSts(){
    return b_facedetect_done;
}

void Dialog_Cam::frameReady(QPixmap pixmap)
{
    if(pixmap.isNull()){
        return ;
    }

	if(b_peoplecnt_en){
        frame_capture = pixmap;
	}
    //Draw Rect on original frame
    if(b_peoplecnt_en){
        QPainter painter(&pixmap);
        QPen pen(Qt::red, 5);
        painter.setPen(pen);
        for(auto d : data){
            int x = d.first.x * mappingfactor_width;
            int w = d.first.width * mappingfactor_width;
            int y = d.first.y * mappingfactor_height;
            int h = d.first.height * mappingfactor_height;
            painter.drawRect(x,y,w,h);
        }
    }
    QPixmap  fitPixmap =  pixmap.scaled ( 800 , 600 , Qt :: KeepAspectRatio , Qt :: SmoothTransformation );
    ui->label_cam_image->setPixmap(fitPixmap);
}

int Dialog_Cam::face_detect_alg(){
    int cnt = 0;
	cv::Mat image;

    b_facedetect_done = false;
    frame_capture =  frame_capture.scaled ( FRAME_DETECTION_X , FRAME_DETECTION_Y , Qt :: KeepAspectRatio , Qt :: SmoothTransformation );
    image = QPixmapToCvMat( frame_capture, true );
	MTCNNDetector detector(pConfig, rConfig, oConfig);
    std::vector<Face> faces;
    {
        faces = detector.detect(image, 20.f, 0.709f);
    }
	cnt = faces.size();// people count number

    // clean previous face detection result
    data.clear();
    for (int i = 0; i < cnt; ++i) {
        std::vector<cv::Point> pts;
        auto rect = faces[i].bbox.getRect();
        auto d = std::make_pair(rect, pts);
        data.push_back(d);
    }

    // display people count number
	ui->lcdNumber_people_cnt->display(cnt);

    b_facedetect_done = true;
	return 0;
}

void Dialog_Cam::on_pushButton_people_cnt_clicked()
{
    b_peoplecnt_en = !b_peoplecnt_en;
    int idx = 0;

    if(b_peoplecnt_en){
        camera->stop();
        ui->pushButton_people_cnt->setText("STOP");
        QCameraViewfinderSettings set;
        idx = find_resolution_idx(PEOPLECNT_RESOLUTION);
        idx_resolution_facedetect = idx;
        if(idx<0){
            qDebug()<<"idx error";
            return;
        }
        mappingfactor_width = (float)mResSize[idx_resolution_display].width()/mResSize[idx_resolution_facedetect].width();
        mappingfactor_height = (float)mResSize[idx_resolution_display].height()/mResSize[idx_resolution_facedetect].height();
        set.setResolution(mResSize[idx_resolution_display]);
        camera->setViewfinderSettings(set);
        ui->comboBox_resolution->setCurrentIndex(idx_resolution_display);

        ui->comboBox_resolution->setDisabled(true);
        ui->comboBox_cam_select->setDisabled(true);

        thread_people_cnt->start();
        camera->start();
    }
    else{
        ui->pushButton_people_cnt->setText("START");
        thread_people_cnt->stop();
        ui->comboBox_resolution->setDisabled(false);
        ui->comboBox_cam_select->setDisabled(false);
        ui->lcdNumber_people_cnt->display(0);
    }
}
