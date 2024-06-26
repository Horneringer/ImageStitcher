#include "gdal/ogr_core.h"

#include <QDebug>
#include <QFileInfo>
#include <QObject>
#include <QPair>
#include <gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/gdal.h>
#include <gdal/gdal_priv.h>
#include <gdal/gdalwarper.h>
#include <gdal/ogr_feature.h>
#include <gdal/ogr_geometry.h>
#include <geodesic.h>
#include <gepcalcmodule/FcgiResponser.hpp>
#include <gepclientmodule/modules/suitabilityCalculator/SuitabilityCalculator.hpp>
#include <gepclientmodule/sysManageDB/SysManageDB.hpp>
#include <ogrsf_frmts.h>
#include <proj.h>

/*!
 * @file imageStitcher.hpp
 * @author Гуреев В.А. (vgureev@npomis.ru)
 * @date Create: 27.02.2024
 * @brief imageStitcher.hpp - Класс сервиса сшивки растровых снимков
 *
 * Данный файл содержит в себе методы сшивки растровых файлов формата GTiff с
 * наилучшим качеством и последующей обрезки по зоне размером 5 на 5 км.
 * @copyright 2024 MIS8
 */

class ImageStitcher : public QObject
{
	Q_OBJECT

public:

	explicit ImageStitcher(QObject* parent = nullptr);
	virtual ~ImageStitcher();

	void run(QStringList queryString);

private:

	///умный указатель, управляющий датасетом
	GDALDatasetUniquePtr p_data_set;

	OGRPoint point_center_go;

	CM::t_paramSUBD		 _data;
	CM::SysManageDB*	 _subd;
	FCGI::FcgiResponser* _resp;

	double				  lat;
	double				  lon;
	QString				  save_path;
	QString				  tmpSavePath;
	bool				  write_subd;
	int					  group_obj_id;
	int					  source_data_type_id;
	QStringList			  paths_tiff;
	QPair<double, double> pixelSize;

	///проверить права на запись в файла
	bool checkWritePermission(const QString& filePath);

	/*!
	 * @brief convertTiffTo3857 - метод конвертирует снимок в в EPSG:3857, если
	 * необходимо
	 * @param paths_to_tiff - список исходных снимков
	 * @return вектор пар с путём и флагом, котрый говорит о том, что путь был
	 * сгенерирован
	 */
	QVector<QPair<QString, bool>> convertTiffTo3857(QStringList paths_to_tiff);

	///получить проекцию снимка
	const char* getEPSGCode(const std::string& projectionString);

	/*!
	 * \brief bufferZone - метод создаёт буфферную зону вокруг заданной точки
	 * \param point - координаты точки ГО
	 * \return координаты точки в EPSG:3857
	 */
	QPair<QPointF, QPointF> bufferZone(OGRPoint point);

	///прямая геодезическая задача
	QPointF geo_direct(QPointF point, double azimuth, double l);

	///открыть расторвые данные
	GDALDatasetUniquePtr openRaster(const char*);

	///определение наилучшего качества среди набора снимков
	QPair<double, double> bestResolution(
	  QVector<QPair<QString, bool>>& paths_to_tiff);

	bool connectSubd();

	bool checkCountArgs(QStringList queryString);

	///проверить существование входных файлов
	bool checkExistenceInputFiles();

	///проверить права на чтение файлов
	bool checkInputAccess();

	bool checkInputType();

	bool checkOutputAccess();

	///метод сшивания снимков
	bool warpImages();

	///добавить записи в СУБД(использовать метод из gepclienmodule)
	bool writeToSUBD();

	/*!
	 * @brief convertTo4326 - сменить проекцию сшитого снимка на EPSG:4326
	 * @return true - успешно, false - ошибка конвертации
	 */
	bool convertTo4326();
};
