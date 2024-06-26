#include "imageStitcher.hpp"

ImageStitcher::ImageStitcher(QObject* parent)
	: QObject(parent),
	  p_data_set(nullptr)
{
	_resp = new FCGI::FcgiResponser(this);
	GDALAllRegister();
}

ImageStitcher::~ImageStitcher() {}

void ImageStitcher::run(QStringList queryString)
{
	try
	{
		if (checkCountArgs(queryString) == false)
			_resp->sendBadRequest();
		else if (connectSubd() == false)
			_resp->sendNetworkAuthenticationRequire();
		else if (checkExistenceInputFiles() == false)
			_resp->sendNotFound();
		else if (checkInputAccess() == false)
			_resp->sendForbiddenRequest();
		else if (checkInputType() == false)
			_resp->sendUnsupportedType();
		else if (checkOutputAccess() == false)
			_resp->sendForbiddenRequest();
		else if (warpImages() != false || convertTo4326() != false
				 || writeToSUBD() == false)
			_resp->sendInternalServerError();
		else
			_resp->sendSuccesResponse();
	}
	catch (...)
	{
		_resp->sendInternalServerError();
	}
}

bool ImageStitcher::checkExistenceInputFiles()
{
	for (const QString& file : paths_tiff)
		if (!QFileInfo::exists(file) || !QFileInfo(file).isFile())
			return false;
	return true;
}

bool ImageStitcher::checkInputAccess()
{
	for (const QString& file : paths_tiff)
		if (!QFileInfo(file).isReadable())
			return false;
	return true;
}

bool ImageStitcher::checkInputType()
{
	for (const QString& file : paths_tiff)
	{
		QString type = file.split(".").last();
		if (type == "tif" || type == "tiff" || type == "geotiff")
			return (true);
	}
	return (false);
}

bool ImageStitcher::checkOutputAccess()
{
	QString absPath = QFileInfo(save_path).absolutePath();
	QDir	dir(absPath);

	if (!dir.exists())
		dir.mkpath(absPath);

	return QFileInfo(absPath).isWritable();
}

bool ImageStitcher::checkWritePermission(const QString& filePath)
{
	QFile file(filePath);
	return file.permissions().testFlag(QFile::WriteUser);
}

QVector<QPair<QString, bool>> ImageStitcher::convertTiffTo3857(
  QStringList paths_tiff)
{
	QVector<QPair<QString, bool>> ret;

	for (QString& path : paths_tiff)
	{
		///открыть растр
		GDALDatasetUniquePtr p_data_set = openRaster(path.toLatin1().data());

		///определить текущую проекцию
		const char* current_ref = p_data_set->GetProjectionRef();

		///определить EPSG код проекции
		QString epsg = "EPSG:" + QString(getEPSGCode(current_ref));

		QString path_3857 =
		  QFileInfo(path).baseName().append("_3857").append(".").append(
			QFileInfo(path).suffix());

		QFileInfo path_info(path);
		QString	  dir = path_info.absolutePath();
		path_3857	  = QDir(dir).filePath(path_3857);

		if (!QString(epsg).contains("3857"))
		{
			///перевести в 3857
			QProcess converter;
			converter.setProgram("gdalwarp");

			QStringList arguments;
			arguments << "-s_srs" << epsg;
			arguments << "-t_srs"
					  << "EPSG:3857";
			arguments << path << path_3857;
			converter.setArguments(arguments);

			converter.start();
			converter.waitForFinished(-1);

			int exit_code = converter.exitCode();

			if (!exit_code)
				ret << QPair<QString, bool>(path_3857, true);

			else
				return QVector<QPair<QString, bool>>();
		}
		else
			ret << QPair<QString, bool>(path, false);
	}
	return ret;
}

const char* ImageStitcher::getEPSGCode(const std::string& projectionString)
{
	OGRSpatialReference projection;

	const char* projectionChars = const_cast<char*>(projectionString.c_str());

	if (projection.importFromWkt(&projectionChars) == OGRERR_NONE)
	{
		const char* epsgCode = projection.GetAuthorityCode(NULL);
		if (!epsgCode)
			return nullptr;
		return epsgCode;
	}
	else
	{
		return nullptr;
	}
}

QPair<QPointF, QPointF> ImageStitcher::bufferZone(OGRPoint point)
{
	int l = 5000;

	QPointF ul	 = geo_direct(QPointF(point.getY(), point.getX()), 0, l);
	QPointF tmp1 = geo_direct(ul, -90, l);
	ul.setX(tmp1.y());
	ul.setY(tmp1.x());

	QPointF lr	 = geo_direct(QPointF(point.getY(), point.getX()), 180, l);
	QPointF tmp2 = geo_direct(lr, 90, l);
	lr.setX(tmp2.y());
	lr.setY(tmp2.x());

	PJ_CONTEXT* C;
	PJ*			P;

	C = proj_context_create();
	P = proj_create_crs_to_crs(C, "EPSG:4326", "EPSG:3857", NULL);
	PJ_COORD input_coords, output_coords;

	input_coords  = proj_coord(ul.y(), ul.x(), 0, 0);
	output_coords = proj_trans(P, PJ_FWD, input_coords);
	ul			  = QPointF(output_coords.xy.x, output_coords.xy.y);

	input_coords  = proj_coord(lr.y(), lr.x(), 0, 0);
	output_coords = proj_trans(P, PJ_FWD, input_coords);
	lr			  = QPointF(output_coords.xy.x, output_coords.xy.y);

	proj_destroy(P);
	proj_context_destroy(C);
	return QPair<QPointF, QPointF>(ul, lr);
}

QPointF ImageStitcher::geo_direct(QPointF point, double azimuth, double l)
{
	double lat = 0;
	double lon = 0;

	struct geod_geodesic gs;

	geod_init(&gs, 6378137.0, 1 / 298.257223563);

	geod_direct(&gs, point.x(), point.y(), azimuth, l, &lat, &lon, 0);
	return QPointF(lat, lon);
}

GDALDatasetUniquePtr ImageStitcher::openRaster(const char* input_raster)
{
	///умный указатель для управления растровым датасетом
	GDALDatasetUniquePtr p_data_set;
	///регистрация всех доступных драйверов и форматов данных
	GDALAllRegister();
	///режим доступа к файлу данных
	const GDALAccess eAccess = GA_ReadOnly;

	///открывает растровый файл данных с помощью функции GDALOpen, а затем
	///создает умный указатель
	p_data_set = GDALDatasetUniquePtr(
	  GDALDataset::FromHandle(GDALOpen(input_raster, eAccess)));

	if (!p_data_set)

		return nullptr;

	return p_data_set;
}

QPair<double, double> ImageStitcher::bestResolution(
  QVector<QPair<QString, bool>>& paths_to_tiff)
{
	QPair<double, double> best_resolution;

	///значение лучшего качества
	QVector<QPair<double, double>> resolutions;

	if (paths_to_tiff.isEmpty())
		return QPair<double, double>();

	for (QPair<QString, bool>& tiff : paths_to_tiff)
	{
		///открыть файл tif
		GDALDataset* dataset = reinterpret_cast<GDALDataset*>(
		  GDALOpen(tiff.first.toUtf8().constData(), GA_ReadOnly));
		QFileInfo tiff_info(tiff.first);
		if (dataset == nullptr)

			continue;

		/// получение геотрансформации (информации о пространственном
		/// разрешении)
		double geo_transform[6];
		if (dataset->GetGeoTransform(geo_transform) != CE_None)
		{
			GDALClose(dataset);
			return QPair<double, double>();
		}

		double minPixelSize = 0.7;
		///размер пикселя в метрах по X и Y направлениям
		double pixelSizeX	= geo_transform[1];

		if (pixelSizeX < 0 && pixelSizeX > -1 * minPixelSize)
			pixelSizeX = -1 * minPixelSize;
		else if (pixelSizeX > 0 && pixelSizeX < minPixelSize)
			pixelSizeX = minPixelSize;

		double pixelSizeY = geo_transform[5];

		if (pixelSizeY < 0 && pixelSizeY > -1 * minPixelSize)
			pixelSizeY = -1 * minPixelSize;
		else if (pixelSizeY > 0 && pixelSizeY < minPixelSize)
			pixelSizeY = minPixelSize;

		resolutions.push_back(
		  qMakePair<double, double>(pixelSizeX, pixelSizeY));

		GDALClose(dataset);
	}

	std::sort(resolutions.begin(), resolutions.end());

	best_resolution = resolutions.first();

	return best_resolution;
}

bool ImageStitcher::connectSubd()
{
	if (write_subd == false)
		return true;
	_subd = CM::SysManageDB::instance();

	_data.driver	   = "QPSQL";
	_data.hostName	   = qgetenv("SUBD_HOSTNAME");
	_data.port		   = qgetenv("SUBD_PORT").toInt();
	_data.databaseName = qgetenv("SUBD_DB");
	_data.userName	   = qgetenv("SUBD_USER");
	_data.password	   = qgetenv("SUBD_PASSWORD");

	bool con = _subd->connectSUBD(_data);
	return (con);
}

bool ImageStitcher::checkCountArgs(QStringList queryString)
{
	if (queryString.length() < 6 || queryString.length() > 26)
		return (false);
	lat					= queryString.at(0).toDouble();
	lon					= queryString.at(1).toDouble();
	write_subd			= queryString.at(2).toInt();
	group_obj_id		= queryString.at(3).toInt();
	source_data_type_id = queryString.at(4).toInt();
	save_path			= queryString.at(5);
	tmpSavePath			= save_path + "f";

	for (int i = 6; i < queryString.size(); i++)
		paths_tiff << queryString.at(i);

	return (true);
}

bool ImageStitcher::writeToSUBD()
{
	if (write_subd == false)
		return true;
	CM::SysManageDB::instance()->insertSourceData(group_obj_id,
												  source_data_type_id);
	return true;
}

bool ImageStitcher::convertTo4326()
{
	if (QFile(save_path).exists())
		QFile(save_path).remove();

	QProcess converter;
	converter.setProgram("gdalwarp");

	QStringList arguments;

	arguments << "-srcnodata"
			  << "0.0";
	arguments << "-dstnodata"
			  << "0.0";
	arguments << "-s_srs"
			  << "EPSG:3857";
	arguments << "-t_srs"
			  << "EPSG:4326";
	arguments << "-co"
			  << "COMPRESS=LZW"; // ZSTD
	arguments << "-co"
			  << "BIGTIFF=YES";
	arguments << "-co"
			  << "PREDICTOR=2";
	arguments << "-co"
			  << "TILED=YES";
	arguments << tmpSavePath << save_path;
	converter.setArguments(arguments);

	converter.start();
	converter.waitForFinished(-1);

	int exit_code = converter.exitCode();

	if (QFile(tmpSavePath).exists())
		QFile(tmpSavePath).remove();

	if (!exit_code)
		return false;
	return true;
}

bool ImageStitcher::warpImages()
{
	if (QFile(tmpSavePath).exists())
		QFile(tmpSavePath).remove();
	QVector<QPair<QString, bool>> ret = convertTiffTo3857(paths_tiff);

	QProcess warper;
	warper.setProgram("gdal_merge.py");
	pixelSize = bestResolution(ret);

	point_center_go				   = OGRPoint(lat, lon);
	QPair<QPointF, QPointF> buffer = bufferZone(point_center_go);
	QStringList				arguments;
	arguments << "-a_nodata"
			  << "0.0";
	arguments << "-ot"
			  << "Byte";
	arguments << "-ps" << QString::number(pixelSize.first)
			  << QString::number(pixelSize.second);
	arguments << "-ul_lr" << QString::number(buffer.first.x())
			  << QString::number(buffer.first.y())
			  << QString::number(buffer.second.x())
			  << QString::number(buffer.second.y());
	arguments << "-co"
			  << "COMPRESS=LZW"; // ZSTD
	arguments << "-co"
			  << "BIGTIFF=YES";
	arguments << "-co"
			  << "PREDICTOR=2";
	arguments << "-co"
			  << "TILED=YES";
	arguments << "-o" << tmpSavePath;

	for (QPair<QString, bool>& path : ret)
		arguments << path.first;

	warper.setArguments(arguments);
	warper.start();
	warper.waitForFinished(-1);

	for (QPair<QString, bool>& path : ret)
		if (path.second)
			QFile(path.first).remove();

	return warper.exitCode();
}
