// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#include "precomp.hpp"

#include "trackerCSRTSegmentation.hpp"
#include "trackerCSRTUtils.hpp"
#include "trackerCSRTScaleEstimation.hpp"


//namespace std
namespace cv
{
/**
* \brief Implementation of TrackerModel for CSRT algorithm
*/
class TrackerCSRTModel : public TrackerModel
{
public:
    TrackerCSRTModel(TrackerCSRT::Params /*params*/){}
    ~TrackerCSRTModel(){}
protected:
    void modelEstimationImpl(const std::vector<Mat>& /*responses*/){}
    void modelUpdateImpl(){}
};


class TrackerCSRTImpl : public TrackerCSRT
{
public:
    TrackerCSRTImpl(const TrackerCSRT::Params &parameters = TrackerCSRT::Params());
    void read(const FileNode& fn);
    void write(FileStorage& fs) const;
    
protected:
    TrackerCSRT::Params params;

    bool initImpl(const Mat& image, const Rect2d& boundingBox);
    virtual void setInitialMask(const Mat mask);
    bool updateImpl(const Mat& image, Rect2d& boundingBox);
    double PSRcompute(const Mat &resp,double max_valzz,Point &max_loczz,Rect2f &bounding_box);
    void update_csr_filter(const Mat &image, const Mat &my_mask);
    void update_histograms(const Mat &image, const Rect &region);
    void extract_histograms(const Mat &image, cv::Rect region, Histogram &hf, Histogram &hb);
    std::vector<Mat> create_csr_filter(const std::vector<cv::Mat>
            img_features, const cv::Mat Y, const cv::Mat P);
    //std::vector<Mat> calculate_response(const Mat &image, const std::vector<Mat> filter);
    Mat calculate_response(const Mat &image, const std::vector<Mat> filter);
    Mat get_location_prior(const Rect roi, const Size2f target_size, const Size img_sz);
    Mat segment_region(const Mat &image, const Point2f &object_center,
            const Size2f &template_size, const Size &target_size, float scale_factor);
    Point2f estimate_new_position(const Mat &image);
    Point2f estimate_new_position_detector(const Mat &image);
    std::vector<Mat> get_features(const Mat &patch, const Size2i &feature_size);

private:
    bool check_mask_area(const Mat &mat, const double obj_area);
    float current_scale_factor;
    Mat window;
    Mat yf;
    Rect2f bounding_box;
    std::vector<Mat> csr_filter;
// by zhangzheng in 2018.05.28
    std::vector<Mat> detector;
    int Nq;
    double Tq;
    float Kai;
    float biaszz;
    double alphaD;
    double lamda;
    int Deltat;
    double Beltat;
    std::vector<Mat> hST0;
    int frame;
// by zhangzheng in 2018.05.28

    std::vector<double> meanpsr;
    double psrmean;
    std::vector<float> filter_weights;
    Size2f original_target_size;
    Size2i image_size;
    Size2f template_size;
    Size2i rescaled_template_size;
    float rescale_ratio;
    Point2f object_center;
    DSST dsst;
    Histogram hist_foreground;
    Histogram hist_background;
    double p_b;
    Mat erode_element;
    Mat filter_mask;
    Mat preset_mask;
    Mat default_mask;
    float default_mask_area;
    int cell_size;
};

Ptr<TrackerCSRT> TrackerCSRT::create(const TrackerCSRT::Params &parameters)
{
    return Ptr<TrackerCSRTImpl>(new TrackerCSRTImpl(parameters));
}
Ptr<TrackerCSRT> TrackerCSRT::create()
{
    return Ptr<TrackerCSRTImpl>(new TrackerCSRTImpl());
}
TrackerCSRTImpl::TrackerCSRTImpl(const TrackerCSRT::Params &parameters) :
    params(parameters)
{
    isInit = false;
}

void TrackerCSRTImpl::read(const cv::FileNode& fn)
{
    params.read(fn);
}

void TrackerCSRTImpl::write(cv::FileStorage& fs) const
{
    params.write(fs);
}

void TrackerCSRTImpl::setInitialMask(const Mat mask)
{
    preset_mask = mask;
}

bool TrackerCSRTImpl::check_mask_area(const Mat &mat, const double obj_area)
{
    double threshold = 0.05;
    double mask_area= sum(mat)[0];
    if(mask_area < threshold*obj_area) {
        return false;
    }
    return true;
}

//std::vector<Mat> TrackerCSRTImpl::calculate_response(const Mat &image, const std::vector<Mat> filter)
Mat TrackerCSRTImpl::calculate_response(const Mat &image, const std::vector<Mat> filter)
{
    std::vector<Mat>  reszz;
    //vector<Mat> patch;
    double scalezz[3];
    //scalezz[0]=0.9604;
    //scalezz[1]=0.98;
    //scalezz[2]=1.0;
    //scalezz[3]=1.02 ;
    //scalezz[4]=1.04;
    scalezz[0]=0.98;
    scalezz[1]=1.0;
    scalezz[2]=1.02;
    //for(int izz=0;izz<3;izz++){
    //for(int jzz=0;jzz<3;jzz++){
    Mat patch = get_subwindow(image, object_center, cvFloor(current_scale_factor * template_size.width),cvFloor(current_scale_factor * template_size.height));
    resize(patch, patch, rescaled_template_size, 0, 0, INTER_CUBIC);
    //patch.push_back(patchtemp);


    std::vector<Mat> ftrs = get_features(patch, yf.size());
    std::vector<Mat> Ffeatures = fourier_transform_features(ftrs);
    Mat resp,res;

    if(params.use_channel_weights){
        res = Mat::zeros(Ffeatures[0].size(), CV_32FC2);
        Mat resp_ch;
        Mat mul_mat;
        for(size_t i = 0; i < Ffeatures.size(); ++i) {
            mulSpectrums(Ffeatures[i], filter[i], resp_ch, 0, true);
            res += (resp_ch * filter_weights[i]);
        }
        idft(res, res, DFT_SCALE | DFT_REAL_OUTPUT);
	//reszz.push_back(res);
    } else {
        res = Mat::zeros(Ffeatures[0].size(), CV_32FC2);
        Mat resp_ch;
        for(size_t i = 0; i < Ffeatures.size(); ++i) {
            mulSpectrums(Ffeatures[i], filter[i], resp_ch, 0 , true);
            res = res + resp_ch;
        }
        idft(res, res, DFT_SCALE | DFT_REAL_OUTPUT);
        //reszz.push_back(res);
    }
   //}
//}
//return reszz;
   return res;
}

void TrackerCSRTImpl::update_csr_filter(const Mat &image, const Mat &mask)
{
    Mat patch = get_subwindow(image, object_center, cvFloor(current_scale_factor * template_size.width),
        cvFloor(current_scale_factor * template_size.height));
    resize(patch, patch, rescaled_template_size, 0, 0, INTER_CUBIC);

    std::vector<Mat> ftrs = get_features(patch, yf.size());
    std::vector<Mat> Fftrs = fourier_transform_features(ftrs);
    std::vector<Mat> new_csr_filter = create_csr_filter(Fftrs, yf, mask);
    //calculate per channel weights
    if(params.use_channel_weights) {
        Mat current_resp;
        double max_val;
        float sum_weights = 0;
        std::vector<float> new_filter_weights = std::vector<float>(new_csr_filter.size());
        for(size_t i = 0; i < new_csr_filter.size(); ++i) {
            mulSpectrums(Fftrs[i], new_csr_filter[i], current_resp, 0, true);
            idft(current_resp, current_resp, DFT_SCALE | DFT_REAL_OUTPUT);
            minMaxLoc(current_resp, NULL, &max_val, NULL, NULL);
            sum_weights += static_cast<float>(max_val);
            new_filter_weights[i] = static_cast<float>(max_val);
        }
        //update filter weights with new values
        float updated_sum = 0;
        for(size_t i = 0; i < filter_weights.size(); ++i) {
            filter_weights[i] = filter_weights[i]*(1.0f - params.weights_lr) +
                params.weights_lr * (new_filter_weights[i] / sum_weights);
            updated_sum += filter_weights[i];
        }
        //normalize weights
        for(size_t i = 0; i < filter_weights.size(); ++i) {
            filter_weights[i] /= updated_sum;
        }
    }
    for(size_t i = 0; i < csr_filter.size(); ++i) {
        csr_filter[i] = (1.0f - params.filter_lr)*csr_filter[i] + params.filter_lr * new_csr_filter[i];
    }
    std::vector<Mat>().swap(ftrs);
    std::vector<Mat>().swap(Fftrs);
}


std::vector<Mat> TrackerCSRTImpl::get_features(const Mat &patch, const Size2i &feature_size)
{
    std::vector<Mat> features;
    if (params.use_hog) {
        std::vector<Mat> hog = get_features_hog(patch, cell_size);
        features.insert(features.end(), hog.begin(),
                hog.begin()+params.num_hog_channels_used);
    }
    if (params.use_color_names) {
        std::vector<Mat> cn;
        cn = get_features_cn(patch, feature_size);
        features.insert(features.end(), cn.begin(), cn.end());
    }
    if(params.use_gray) {
        Mat gray_m;
        cvtColor(patch, gray_m, CV_BGR2GRAY);
        resize(gray_m, gray_m, feature_size, 0, 0, INTER_CUBIC);
        gray_m.convertTo(gray_m, CV_32FC1, 1.0/255.0, -0.5);
        features.push_back(gray_m);
        //Mat hsv_img = bgr2hsv(patch);
        //resize(hsv_img, hsv_img, feature_size, 0, 0, INTER_CUBIC);
        //gray_m.convertTo(hsv_img, CV_32FC1, 1.0/255.0, -0.5);
        //features.push_back(hsv_img);
    }
    if(params.use_rgb) {
        std::vector<Mat> rgb_features = get_features_rgb(patch, feature_size);
        features.insert(features.end(), rgb_features.begin(), rgb_features.end());
    }
    /*if(params.use_hsv) {
         Mat hsv_img = bgr2hsv(patch);
         features.push_back(hsv_img);
    }*/
    for (size_t i = 0; i < features.size(); ++i) {
        features.at(i) = features.at(i).mul(window);
    }
    return features;
}

class ParallelCreateCSRFilter : public ParallelLoopBody {
public:
    ParallelCreateCSRFilter(
        const std::vector<cv::Mat> img_features,
        const cv::Mat Y,
        const cv::Mat P,
        int admm_iterations,
        std::vector<Mat> &result_filter_):
        result_filter(result_filter_)
    {
        this->img_features = img_features;
        this->Y = Y;
        this->P = P;
        this->admm_iterations = admm_iterations;
    }
    virtual void operator ()(const Range& range) const
    {
        for (int i = range.start; i < range.end; i++) {
            float mu = 5.0f;
            float beta = 3.0f;
            float mu_max = 20.0f;
            float lambda = mu / 100.0f;

            Mat F = img_features[i];

            Mat Sxy, Sxx;
            mulSpectrums(F, Y, Sxy, 0, true);
            mulSpectrums(F, F, Sxx, 0, true);

            Mat H;
            H = divide_complex_matrices(Sxy, (Sxx + lambda));
            idft(H, H, DFT_SCALE|DFT_REAL_OUTPUT);
            H = H.mul(P);
            dft(H, H, DFT_COMPLEX_OUTPUT);
            Mat L = Mat::zeros(H.size(), H.type()); //Lagrangian multiplier
            Mat G;
            for(int iteration = 0; iteration < admm_iterations; ++iteration) {
                G = divide_complex_matrices((Sxy + (mu * H) - L) , (Sxx + mu));
                idft((mu * G) + L, H, DFT_SCALE | DFT_REAL_OUTPUT);
                float lm = 1.0f / (lambda+mu);
                H = H.mul(P*lm);
                dft(H, H, DFT_COMPLEX_OUTPUT);

                //Update variables for next iteration
                L = L + mu * (G - H);
                mu = min(mu_max, beta*mu);
            }
            result_filter[i] = H;
        }
    }

    ParallelCreateCSRFilter& operator=(const ParallelCreateCSRFilter &) {
        return *this;
    }

private:
    int admm_iterations;
    Mat Y;
    Mat P;
    std::vector<Mat> img_features;
    std::vector<Mat> &result_filter;
};


std::vector<Mat> TrackerCSRTImpl::create_csr_filter(
        const std::vector<cv::Mat> img_features,
        const cv::Mat Y,
        const cv::Mat P)
{
    std::vector<Mat> result_filter;
    result_filter.resize(img_features.size());
    ParallelCreateCSRFilter parallelCreateCSRFilter(img_features, Y, P,
            params.admm_iterations, result_filter);
    parallel_for_(Range(0, static_cast<int>(result_filter.size())), parallelCreateCSRFilter);

    return result_filter;
}

Mat TrackerCSRTImpl::get_location_prior(
        const Rect roi,
        const Size2f target_size,
        const Size img_sz)
{
    int x1 = cvRound(max(min(roi.x-1, img_sz.width-1) , 0));
    int y1 = cvRound(max(min(roi.y-1, img_sz.height-1) , 0));

    int x2 = cvRound(min(max(roi.width-1, 0) , img_sz.width-1));
    int y2 = cvRound(min(max(roi.height-1, 0) , img_sz.height-1));

    Size target_sz;
    target_sz.width = target_sz.height = cvFloor(min(target_size.width, target_size.height));

    double cx = x1 + (x2-x1)/2.;
    double cy = y1 + (y2-y1)/2.;
    double kernel_size_width = 1.0/(0.5*static_cast<double>(target_sz.width)*1.4142+1);
    double kernel_size_height = 1.0/(0.5*static_cast<double>(target_sz.height)*1.4142+1);

    cv::Mat kernel_weight = Mat::zeros(1 + cvFloor(y2 - y1) , 1+cvFloor(-(x1-cx) + (x2-cx)), CV_64FC1);
    for (int y = y1; y < y2+1; ++y){
        double * weightPtr = kernel_weight.ptr<double>(y);
        double tmp_y = std::pow((cy-y)*kernel_size_height, 2);
        for (int x = x1; x < x2+1; ++x){
            weightPtr[x] = kernel_epan(std::pow((cx-x)*kernel_size_width,2) + tmp_y);
        }
    }

    double max_val;
    cv::minMaxLoc(kernel_weight, NULL, &max_val, NULL, NULL);
    Mat fg_prior = kernel_weight / max_val;
    fg_prior.setTo(0.5, fg_prior < 0.5);
    fg_prior.setTo(0.9, fg_prior > 0.9);
    return fg_prior;
}

Mat TrackerCSRTImpl::segment_region(
        const Mat &image,
        const Point2f &object_center,
        const Size2f &template_size,
        const Size &target_size,
        float scale_factor)
{
    Rect valid_pixels;
    Mat patch = get_subwindow(image, object_center, cvFloor(scale_factor * template_size.width),
        cvFloor(scale_factor * template_size.height), &valid_pixels);
    Size2f scaled_target = Size2f(target_size.width * scale_factor,
            target_size.height * scale_factor);
    Mat fg_prior = get_location_prior(
            Rect(0,0, patch.size().width, patch.size().height),
            scaled_target , patch.size());

    std::vector<Mat> img_channels;
    split(patch, img_channels);
    std::pair<Mat, Mat> probs = Segment::computePosteriors2(img_channels, 0, 0, patch.cols, patch.rows,
                    p_b, fg_prior, 1.0-fg_prior, hist_foreground, hist_background);

    Mat mask = Mat::zeros(probs.first.size(), probs.first.type());
    probs.first(valid_pixels).copyTo(mask(valid_pixels));
    double max_resp = get_max(mask);
    threshold(mask, mask, max_resp / 2.0, 1, THRESH_BINARY);
    mask.convertTo(mask, CV_32FC1, 1.0);
    return mask;
}


void TrackerCSRTImpl::extract_histograms(const Mat &image, cv::Rect region, Histogram &hf, Histogram &hb)
{
    // get coordinates of the region
    int x1 = std::min(std::max(0, region.x), image.cols-1);
    int y1 = std::min(std::max(0, region.y), image.rows-1);
    int x2 = std::min(std::max(0, region.x + region.width), image.cols-1);
    int y2 = std::min(std::max(0, region.y + region.height), image.rows-1);

    // calculate coordinates of the background region
    int offsetX = (x2-x1+1) / params.background_ratio;
    int offsetY = (y2-y1+1) / params.background_ratio;
    int outer_y1 = std::max(0, (int)(y1-offsetY));
    int outer_y2 = std::min(image.rows, (int)(y2+offsetY+1));
    int outer_x1 = std::max(0, (int)(x1-offsetX));
    int outer_x2 = std::min(image.cols, (int)(x2+offsetX+1));

    // calculate probability for the background
    p_b = 1.0 - ((x2-x1+1) * (y2-y1+1)) /
        ((double) (outer_x2-outer_x1+1) * (outer_y2-outer_y1+1));

    // split multi-channel image into the std::vector of matrices
    std::vector<Mat> img_channels(image.channels());
    split(image, img_channels);
    for(size_t k=0; k<img_channels.size(); k++) {
        img_channels.at(k).convertTo(img_channels.at(k), CV_8UC1);
    }

    hf.extractForegroundHistogram(img_channels, Mat(), false, x1, y1, x2, y2);
    hb.extractBackGroundHistogram(img_channels, x1, y1, x2, y2,
        outer_x1, outer_y1, outer_x2, outer_y2);
    std::vector<Mat>().swap(img_channels);
}

void TrackerCSRTImpl::update_histograms(const Mat &image, const Rect &region)
{
    // create temporary histograms
    Histogram hf(image.channels(), params.histogram_bins);
    Histogram hb(image.channels(), params.histogram_bins);
    extract_histograms(image, region, hf, hb);

    // get histogram vectors from temporary histograms
    std::vector<double> hf_vect_new = hf.getHistogramVector();
    std::vector<double> hb_vect_new = hb.getHistogramVector();
    // get histogram vectors from learned histograms
    std::vector<double> hf_vect = hist_foreground.getHistogramVector();
    std::vector<double> hb_vect = hist_background.getHistogramVector();

    // update histograms - use learning rate
    for(size_t i=0; i<hf_vect.size(); i++) {
        hf_vect_new[i] = (1-params.histogram_lr)*hf_vect[i] +
            params.histogram_lr*hf_vect_new[i];
        hb_vect_new[i] = (1-params.histogram_lr)*hb_vect[i] +
            params.histogram_lr*hb_vect_new[i];
    }

    // set learned histograms
    hist_foreground.setHistogramVector(&hf_vect_new[0]);
    hist_background.setHistogramVector(&hb_vect_new[0]);

    std::vector<double>().swap(hf_vect);
    std::vector<double>().swap(hb_vect);
}

Point2f TrackerCSRTImpl::estimate_new_position(const Mat &image)
{

    //std::vector<Mat> respzz = calculate_response(image, csr_filter);
    //Mat resp=respzz[4];
    Mat resp=calculate_response(image, csr_filter);
    Point max_loc;
    minMaxLoc(resp, NULL, NULL, NULL, &max_loc);
    // take into account also subpixel accuracy
    float col = ((float) max_loc.x) + subpixel_peak(resp, "horizontal", max_loc);
    float row = ((float) max_loc.y) + subpixel_peak(resp, "vertical", max_loc);
    if(row + 1 > (float)resp.rows / 2.0f) {
        row = row - resp.rows;
    }
    if(col + 1 > (float)resp.cols / 2.0f) {
        col = col - resp.cols;
    }
    // calculate x and y displacements
    Point2f new_center = object_center + Point2f(current_scale_factor * (1.0f / rescale_ratio) *cell_size*(col),
            current_scale_factor * (1.0f / rescale_ratio) *cell_size*(row));
    //sanity checks
    if(new_center.x < 0)
        new_center.x = 0;
    if(new_center.x >= image_size.width)
        new_center.x = static_cast<float>(image_size.width - 1);
    if(new_center.y < 0)
        new_center.y = 0;
    if(new_center.y >= image_size.height)
        new_center.y = static_cast<float>(image_size.height - 1);

    return new_center;
}

Point2f TrackerCSRTImpl::estimate_new_position_detector(const Mat &image)
{

    //std::vector<Mat> respzz = calculate_response(image,detector);
    //Mat resp=respzz[4];
    Mat resp = calculate_response(image,detector);
    Point max_loc;
    minMaxLoc(resp, NULL, NULL, NULL, &max_loc);
    // take into account also subpixel accuracy
    float col = ((float) max_loc.x) + subpixel_peak(resp, "horizontal", max_loc);
    float row = ((float) max_loc.y) + subpixel_peak(resp, "vertical", max_loc);
    if(row + 1 > (float)resp.rows / 2.0f) {
        row = row - resp.rows;
    }
    if(col + 1 > (float)resp.cols / 2.0f) {
        col = col - resp.cols;
    }
    // calculate x and y displacements
    Point2f new_center = object_center + Point2f(current_scale_factor * (1.0f / rescale_ratio) *cell_size*(col),
            current_scale_factor * (1.0f / rescale_ratio) *cell_size*(row));
    //sanity checks
    if(new_center.x < 0)
        new_center.x = 0;
    if(new_center.x >= image_size.width)
        new_center.x = static_cast<float>(image_size.width - 1);
    if(new_center.y < 0)
        new_center.y = 0;
    if(new_center.y >= image_size.height)
        new_center.y = static_cast<float>(image_size.height - 1);

    return new_center;
}

// *********************************************************************
// *                        Update API function                        *
// *********************************************************************
bool TrackerCSRTImpl::updateImpl(const Mat& image_, Rect2d& boundingBox)
{
    //treat gray image as color image
    Mat image;
    if(image_.channels() == 1) {
        std::vector<Mat> channels(3);
        channels[0] = channels[1] = channels[2] = image_;
        merge(channels, image);
    } else {
        image = image_;
    }
    //std::vector<Mat> respzz = calculate_response(image, csr_filter);
    Mat resp = calculate_response(image, csr_filter);
    double max_valzz;
    	
	
    double scalezz[3];
   /* scalezz[0]=0.9604;
    scalezz[1]=0.98;
    scalezz[2]=1.0;
    scalezz[3]=1.02 ;
    scalezz[4]=1.04;*/
    scalezz[0]=0.98;
    scalezz[1]=1.0;
    scalezz[2]=1.02 ;
    Point max_loczz;
    minMaxLoc(resp, NULL, &max_valzz, NULL, &max_loczz);
/*   int izzfinal=2, jzzfinal=2;
   double maxfinal;
   Point max_final;
   double maxtemp=0.0;
   for(int izz=0;izz<3; izz++)
   {
	for(int jzz=0;jzz< 3; jzz++)
	{
	     int tempzz=jzz+izz*3;
     
		minMaxLoc(respzz[tempzz], NULL, &max_valzz, NULL , &max_final);
        if(max_valzz>maxtemp)
	{
		max_valzz=maxtemp;
                izzfinal=izz;
   		jzzfinal=jzz;
                max_loczz=max_final;
	}
        // normalize response
//float col = ((float) max_loc.x) 
}}
        Mat resp;
        resp.push_back(respzz[jzzfinal+izzfinal*3]);*/
        Scalar mean,std;
        meanStdDev(resp, mean, std);
        double PSR=1000.0;
        double epszz=0.00001;  
        double temp1=0.01;

 /*float col = ((float) max_loc.x) + subpixel_peak(resp, "horizontal", max_loc);
    float row = ((float) max_loc.y) + subpixel_peak(resp, "vertical", max_loc);
    if(row + 1 > (float)resp.rows / 2.0f) {
        row = row - resp.rows;
    }
    if(col + 1 > (float)resp.cols / 2.0f) {
        col = col - resp.cols;
    }*/
 	Mat resp1=Mat::zeros(resp.rows, resp.cols, CV_32FC2);

	for(int i=0;i<resp.rows;i++)
	{
		for(int j=0;j<resp.cols;j++)
		{
			if(i+1<resp.rows/2.0f&& j+1<resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i+int(resp.rows/2),j+int(resp.cols/2));
			}
			if(i+1<resp.rows/2.0f&& j+1>=resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i+int(resp.rows/2),j-int(resp.cols/2));
			}
if(i+1>=resp.rows/2.0f&& j+1<resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i-int(resp.rows/2),j+int(resp.cols/2));
			}
			if(i+1>=resp.rows/2.0f&& j+1>=resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i-int(resp.rows/2),j-int(resp.cols/2));
			}
			
		}
	}




	//float * ptr1=(float *)(resp.data);
	for(int i=0;i<resp1.rows;i++)
	{
		for(int j=0;j<resp1.cols;j++)
		{
			if(!((i==max_loczz.y)&&(j==max_loczz.x))){
				//temp1=((double)(max_valzz-ptr1[0]))/(1-exp(-(4/sqrt((bounding_box.width*bounding_box.height))*((i-max_loczz.y)*(i-max_loczz.y)+(j-max_loczz.x)*(j-max_loczz.x)))));
temp1=(exp(biaszz+(double)(max_valzz-resp1.at<float>(i,j))))/(1-exp(-(Kai/sqrt((bounding_box.width*bounding_box.height))*((i-max_loczz.y)*(i-max_loczz.y)+(j-max_loczz.x)*(j-max_loczz.x)))));
				if(temp1<PSR){ PSR=temp1;}
			}
		//ptr1++;	
		}
		
	}
	if(abs(PSR)<0.000001)
        {
        	PSR = (max_valzz-mean[0]) / (std[0]+epszz); // PSR
	}
        double qSTt;
        
        //double temp;
        //temp=PSR*max_valzz;
        qSTt = PSR*max_valzz;
	frame++;
	meanpsr.push_back(qSTt);
	
	//else if(PSRtemp<Tq) //by zhangzheng 2018.5.28
    	
        Mat tempzz=Mat(meanpsr);
        //Scalar meanpsrzz=mean(meanpsr);
        //double sum=std::accumulate(std::begin(meanpsr),std::end(meanpsr),0.0);
	Scalar meanpsrzz;
      
        if(meanpsr.size()<=100)
        {
		 meanStdDev(tempzz,meanpsrzz,std);
	}
	else
	{
		 //meanStdDev(tempzz(meanpsr.size()-100:meanpsr.size()-1),meanpsrzz,std);
                 //double sum=accumulate(meanpsr.end()-99:meanpsr.end);
		double sum=0.0;
		for(int i=meanpsr.size()-100;i<meanpsr.size();i++)
		{
			sum+=meanpsr[i];
		}
		 meanpsrzz=sum/100;
                 // meanpsrzz=mean(meanpsr(meanpsr.size()-100:meanpsr.size()-1));
	}
        //
        //double meanpsrzz=sum/meanpsr.size();
        //double PSRtemp=meanpsrzz[0]/(PSR*max_valzz);
        double PSRtemp=(PSR*max_valzz)/meanpsrzz[0];
        if(PSRtemp>=Tq && frame!=2) //by zhangzheng 2018.5.28
    	{
        	meanpsr.pop_back();
	}
	//PSRtemp=meanpsrzz/(PSR*max_valzz);
    object_center = estimate_new_position(image);
    
    current_scale_factor = dsst.getScale(image, object_center);
    //update bouding_box according to new scale and location
    bounding_box.x = object_center.x - current_scale_factor * original_target_size.width / 2.0f;
    bounding_box.y = object_center.y - current_scale_factor * original_target_size.height / 2.0f;
  //  bounding_box.width = current_scale_factor * original_target_size.width;
  //  bounding_box.height = current_scale_factor * original_target_size.height*scalezz[izzfinal];
    bounding_box.width = current_scale_factor * original_target_size.width;
    bounding_box.height = current_scale_factor * original_target_size.height;

    //update tracker
    if(params.use_segmentation) {
        Mat hsv_img = bgr2hsv(image);
        update_histograms(hsv_img, bounding_box);
        filter_mask = segment_region(hsv_img, object_center,
                template_size,original_target_size, current_scale_factor);
        resize(filter_mask, filter_mask, yf.size(), 0, 0, INTER_NEAREST);
        if(check_mask_area(filter_mask, default_mask_area)) {
            dilate(filter_mask , filter_mask, erode_element);
        } else {
            filter_mask = default_mask;
        }
    } else {
        filter_mask = default_mask;
    }
	
    if(PSRtemp<Tq) //by zhangzheng 2018.5.28
    {
    	update_csr_filter(image, filter_mask);
    }
    else if(PSRtemp>=Tq)
    {
	Deltat++;
	Beltat=std::exp((-alphaD)*(double(Deltat)));
	for(size_t i = 0; i < csr_filter.size(); ++i) {
             //csr_filter[i] = (1.0f - Beltat)*hST0[i] + Beltat * detector[i];
             detector[i] = (1.0f - Beltat)*hST0[i] + Beltat * csr_filter[i];
         }
        //Detector = (1-Beltat).*hST0+Beltat.*csr_filter;
        //std::vector<Mat> resp = calculate_response(image, detector);
	Mat resp = calculate_response(image, detector);
	//meanStdDev(resp, mean, std); 
        minMaxLoc(resp, NULL, &max_valzz, NULL, &max_loczz);
        PSR=PSRcompute(resp,max_valzz, max_loczz,bounding_box);
       
        //PSR = (max_valzz-mean[0]) / (std[0]+epszz); // PSR
        //double temp;
        //temp=PSR*max_valzz;

        //if(PSR*max_valzz<=qSTt)
   	if(PSR*max_valzz<=meanpsrzz[0])
	{
		
	}
	else
	{
		object_center = estimate_new_position_detector(image);
	}
    }
    dsst.update(image, object_center);
    boundingBox = bounding_box;
    return true;
}
double TrackerCSRTImpl::PSRcompute(const Mat &resp,double max_valzz,Point &max_loczz, Rect2f &bounding_box)
{
	Mat resp1=Mat::zeros(resp.rows, resp.cols, CV_32FC2);

	for(int i=0;i<resp.rows;i++)
	{
		for(int j=0;j<resp.cols;j++)
		{
			if(i+1<resp.rows/2.0f&& j+1<resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i+int(resp.rows/2),j+int(resp.cols/2));
			}
			if(i+1<resp.rows/2.0f&& j+1>=resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i+int(resp.rows/2),j-int(resp.cols/2));
			}
			if(i+1>=resp.rows/2.0f&& j+1<resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i-int(resp.rows/2),j+int(resp.cols/2));
			}
			if(i+1>=resp.rows/2.0f&& j+1>=resp.cols/2.0f)
			{
				resp1.at<float>(i,j)=resp.at<float>(i-int(resp.rows/2),j-int(resp.cols/2));
			}
			
		}
	}
	//float * ptr1=(float *)(resp.data);
	double temp1=0.0,PSR=10000.0;
	
	for(int i=0;i<resp1.rows;i++)
	{
		for(int j=0;j<resp1.cols;j++)
		{
			if(!((i==max_loczz.y)&&(j==max_loczz.x))){
temp1=(exp(biaszz+(double)(max_valzz-resp1.at<float>(i,j))))/(1-exp(-(Kai/sqrt((bounding_box.width*bounding_box.height))*((i-max_loczz.y)*(i-max_loczz.y)+(j-max_loczz.x)*(j-max_loczz.x)))));
				if(temp1<PSR){ PSR=temp1;}
			}
		//ptr1++;	
		}
		
	}
	return PSR;
}

// *********************************************************************
// *                        Init API function                          *
// *********************************************************************
bool TrackerCSRTImpl::initImpl(const Mat& image_, const Rect2d& boundingBox)
{
    cv::setNumThreads(getNumThreads());

    //treat gray image as color image
    Mat image;
    if(image_.channels() == 1) {
        std::vector<Mat> channels(3);
        channels[0] = channels[1] = channels[2] = image_;
        merge(channels, image);
    } else {
        image = image_;
    }

    current_scale_factor = 1.0;
    image_size = image.size();
    bounding_box = boundingBox;
    cell_size = cvFloor(std::min(4.0, std::max(1.0, static_cast<double>(
        cvCeil((bounding_box.width * bounding_box.height)/400.0)))));
    original_target_size = Size(bounding_box.size());

    template_size.width = static_cast<float>(cvFloor(original_target_size.width + params.padding *
            sqrt(original_target_size.width * original_target_size.height)));
    template_size.height = static_cast<float>(cvFloor(original_target_size.height + params.padding *
            sqrt(original_target_size.width * original_target_size.height)));
    template_size.width = template_size.height =
        (template_size.width + template_size.height) / 2.0f;
    rescale_ratio = sqrt(pow(params.template_size,2) / (template_size.width * template_size.height));
    if(rescale_ratio > 1)  {
        rescale_ratio = 1;
    }
    rescaled_template_size = Size2i(cvFloor(template_size.width * rescale_ratio),
            cvFloor(template_size.height * rescale_ratio));
    object_center = Point2f(static_cast<float>(boundingBox.x) + original_target_size.width / 2.0f,
            static_cast<float>(boundingBox.y) + original_target_size.height / 2.0f);

    yf = gaussian_shaped_labels(params.gsl_sigma,
            rescaled_template_size.width / cell_size, rescaled_template_size.height / cell_size);
    if(params.window_function.compare("hann") == 0) {
        window = get_hann_win(Size(yf.cols,yf.rows));
    } else if(params.window_function.compare("cheb") == 0) {
        window = get_chebyshev_win(Size(yf.cols,yf.rows), params.cheb_attenuation);
    } else if(params.window_function.compare("kaiser") == 0) {
        window = get_kaiser_win(Size(yf.cols,yf.rows), params.kaiser_alpha);
    } else {
        std::cout << "Not a valid window function" << std::endl;
        return false;
    }

    Size2i scaled_obj_size = Size2i(cvFloor(original_target_size.width * rescale_ratio / cell_size),
            cvFloor(original_target_size.height * rescale_ratio / cell_size));
    //set dummy mask and area;
    int x0 = std::max((yf.size().width - scaled_obj_size.width)/2 - 1, 0);
    int y0 = std::max((yf.size().height - scaled_obj_size.height)/2 - 1, 0);
    default_mask = Mat::zeros(yf.size(), CV_32FC1);
    default_mask(Rect(x0,y0,scaled_obj_size.width, scaled_obj_size.height)) = 1.0f;
    default_mask_area = static_cast<float>(sum(default_mask)[0]);

    //initalize segmentation
    if(params.use_segmentation) {
        Mat hsv_img = bgr2hsv(image);
        hist_foreground = Histogram(hsv_img.channels(), params.histogram_bins);
        hist_background = Histogram(hsv_img.channels(), params.histogram_bins);
        extract_histograms(hsv_img, bounding_box, hist_foreground, hist_background);
        filter_mask = segment_region(hsv_img, object_center, template_size,
                original_target_size, current_scale_factor);
        //update calculated mask with preset mask
        if(preset_mask.data){
            Mat preset_mask_padded = Mat::zeros(filter_mask.size(), filter_mask.type());
            int sx = std::max((int)cvFloor(preset_mask_padded.cols / 2.0f - preset_mask.cols / 2.0f) - 1, 0);
            int sy = std::max((int)cvFloor(preset_mask_padded.rows / 2.0f - preset_mask.rows / 2.0f) - 1, 0);
            preset_mask.copyTo(preset_mask_padded(
                        Rect(sx, sy, preset_mask.cols, preset_mask.rows)));
            filter_mask = filter_mask.mul(preset_mask_padded);
        }
        erode_element = getStructuringElement(MORPH_ELLIPSE, Size(3,3), Point(1,1));
        resize(filter_mask, filter_mask, yf.size(), 0, 0, INTER_NEAREST);
        if(check_mask_area(filter_mask, default_mask_area)) {
            dilate(filter_mask , filter_mask, erode_element);
        } else {
            filter_mask = default_mask;
        }

    } else {
        filter_mask = default_mask;
    }

    //initialize filter
    Mat patch = get_subwindow(image, object_center, cvFloor(current_scale_factor * template_size.width),
        cvFloor(current_scale_factor * template_size.height));
    resize(patch, patch, rescaled_template_size, 0, 0, INTER_CUBIC);
    std::vector<Mat> patch_ftrs = get_features(patch, yf.size());
    std::vector<Mat> Fftrs = fourier_transform_features(patch_ftrs);
    csr_filter = create_csr_filter(Fftrs, yf, filter_mask);

    detector= csr_filter; //zhangzehng in 2018.5.28
    hST0=csr_filter;
    Deltat=0;
    Beltat=std::exp((-alphaD)*(double(Deltat)));
  
    //  Tq=15;
    Tq=3.006125;
    Kai=11.01;
    biaszz=0.3625;
    frame=1;
   
    if(params.use_channel_weights) {
        Mat current_resp;
        filter_weights = std::vector<float>(csr_filter.size());
        float chw_sum = 0;
        for (size_t i = 0; i < csr_filter.size(); ++i) {
            mulSpectrums(Fftrs[i], csr_filter[i], current_resp, 0, true);
            idft(current_resp, current_resp, DFT_SCALE | DFT_REAL_OUTPUT);
            double max_val;
            minMaxLoc(current_resp, NULL, &max_val, NULL , NULL);
            chw_sum += static_cast<float>(max_val);
            filter_weights[i] = static_cast<float>(max_val);
        }
        for (size_t i = 0; i < filter_weights.size(); ++i) {
            filter_weights[i] /= chw_sum;
        }
    }

    //initialize scale search
    dsst = DSST(image, bounding_box, template_size, params.number_of_scales, params.scale_step,
            params.scale_model_max_area, params.scale_sigma_factor, params.scale_lr);

    model=Ptr<TrackerCSRTModel>(new TrackerCSRTModel(params));
    isInit = true;
    return true;
}

TrackerCSRT::Params::Params()
{
    use_channel_weights = true;
    use_segmentation = true;
    use_hog = true;
    use_color_names = true;
    use_gray = true;
    //use_color_names = false;
    //use_gray = true;
    use_rgb = false;
   //use_hsv = true;
    window_function = "hann";
    //window_function = "gaussian";
    kaiser_alpha = 3.72f;
  
    cheb_attenuation = 45;
    padding = 3.2f;
    template_size = 200; 
    gsl_sigma = 1.0f;
    hog_orientations = 9;
    hog_clip = 0.15f;
    num_hog_channels_used = 18;
    filter_lr = 0.02f;	 
    weights_lr = 0.02f;
    admm_iterations = 4;
    number_of_scales = 33;
    scale_sigma_factor = 0.250f;
    scale_model_max_area = 512.0f;
    scale_lr = 0.025f;
    scale_step = 1.020f;
    histogram_bins = 16;
    background_ratio = 2;
    histogram_lr = 0.04f;
   
	
}

void TrackerCSRT::Params::read(const FileNode& fn)
{
    *this = TrackerCSRT::Params();
    if(!fn["padding"].empty())
        fn["padding"] >> padding;
    if(!fn["template_size"].empty())
        fn["template_size"] >> template_size;
    if(!fn["gsl_sigma"].empty())
        fn["gsl_sigma"] >> gsl_sigma;
    if(!fn["hog_orientations"].empty())
        fn["hog_orientations"] >> hog_orientations;
    if(!fn["num_hog_channels_used"].empty())
        fn["num_hog_channels_used"] >> num_hog_channels_used;
    if(!fn["hog_clip"].empty())
        fn["hog_clip"] >> hog_clip;
    if(!fn["use_hog"].empty())
        fn["use_hog"] >> use_hog;
    if(!fn["use_color_names"].empty())
        fn["use_color_names"] >> use_color_names;
    if(!fn["use_gray"].empty())
        fn["use_gray"] >> use_gray;
    if(!fn["use_rgb"].empty())
        fn["use_rgb"] >> use_rgb;
    //if(!fn["use_hsv"].empty())
    //    fn["use_hsv"] >> use_hsv;
    if(!fn["window_function"].empty())
        fn["window_function"] >> window_function;
    if(!fn["kaiser_alpha"].empty())
        fn["kaiser_alpha"] >> kaiser_alpha;
    if(!fn["cheb_attenuation"].empty())
        fn["cheb_attenuation"] >> cheb_attenuation;
    if(!fn["filter_lr"].empty())
        fn["filter_lr"] >> filter_lr;
    if(!fn["admm_iterations"].empty())
        fn["admm_iterations"] >> admm_iterations;
    if(!fn["number_of_scales"].empty())
        fn["number_of_scales"] >> number_of_scales;
    if(!fn["scale_sigma_factor"].empty())
        fn["scale_sigma_factor"] >> scale_sigma_factor;
    if(!fn["scale_model_max_area"].empty())
        fn["scale_model_max_area"] >> scale_model_max_area;
    if(!fn["scale_lr"].empty())
        fn["scale_lr"] >> scale_lr;
    if(!fn["scale_step"].empty())
        fn["scale_step"] >> scale_step;
    if(!fn["use_channel_weights"].empty())
        fn["use_channel_weights"] >> use_channel_weights;
    if(!fn["weights_lr"].empty())
        fn["weights_lr"] >> weights_lr;
    if(!fn["use_segmentation"].empty())
        fn["use_segmentation"] >> use_segmentation;
    if(!fn["histogram_bins"].empty())
        fn["histogram_bins"] >> histogram_bins;
    if(!fn["background_ratio"].empty())
        fn["background_ratio"] >> background_ratio;
    if(!fn["histogram_lr"].empty())
        fn["histogram_lr"] >> histogram_lr;
    CV_Assert(number_of_scales % 2 == 1);
    CV_Assert(use_gray || use_color_names || use_hog || use_rgb);
}
void TrackerCSRT::Params::write(FileStorage& fs) const
{
    fs << "padding" << padding;
    fs << "template_size" << template_size;
    fs << "gsl_sigma" << gsl_sigma;
    fs << "hog_orientations" << hog_orientations;
    fs << "num_hog_channels_used" << num_hog_channels_used;
    fs << "hog_clip" << hog_clip;
    fs << "use_hog" << use_hog;
    fs << "use_color_names" << use_color_names;
    fs << "use_gray" << use_gray;
    fs << "use_rgb" << use_rgb;
    //fs << "use_hsv" << use_hsv;
    fs << "window_function" << window_function;
    fs << "kaiser_alpha" << kaiser_alpha;
    fs << "cheb_attenuation" << cheb_attenuation;
    fs << "filter_lr" << filter_lr;
    fs << "admm_iterations" << admm_iterations;
    fs << "number_of_scales" << number_of_scales;
    fs << "scale_sigma_factor" << scale_sigma_factor;
    fs << "scale_model_max_area" << scale_model_max_area;
    fs << "scale_lr" << scale_lr;
    fs << "scale_step" << scale_step;
    fs << "use_channel_weights" << use_channel_weights;
    fs << "weights_lr" << weights_lr;
    fs << "use_segmentation" << use_segmentation;
    fs << "histogram_bins" << histogram_bins;
    fs << "background_ratio" << background_ratio;
    fs << "histogram_lr" << histogram_lr;
}
} /* namespace cv */
