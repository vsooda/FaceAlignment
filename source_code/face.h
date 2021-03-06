#ifndef FACE_H_
#define FACE_H_

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "cv.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <ctime>
#include <string>
#include <limits>
#include <algorithm>
#include <cmath>
#include <vector>
#include <fstream>
#include <numeric>   
using namespace std;
using namespace cv;


class Bbox{
    public:
        double start_x;
        double start_y;
        double width;
        double height;
        double centroid_x;
        double centroid_y;
        Bbox(){
            start_x = 0;
            start_y = 0;
            width = 0;
            height = 0;
            centroid_x = 0;
            centroid_y = 0;  
        };
};

class SimilarityTransform{
    public:
        double scale;
        Mat_<double> rotation;
        SimilarityTransform(){
            scale = 1;
            rotation = Mat::zeros(2,2,CV_64FC1);
        };
        Mat_<double> apply_similarity_transform(const Mat_<double>& shape){
            Mat_<double> result = shape.clone();
            Mat_<double> temp;
            transpose(rotation,temp);
            result = scale * shape * temp;        
            return result;
        };
};


class Fern{
    private:
        int pixel_pair_num_in_fern_;
        int landmark_num_;
        Mat_<int> nearest_keypoint_index_;
        Mat_<double> threshold_;
        Mat_<double> selected_x_;
        Mat_<double> selected_y_;
        Mat_<double> pixel_coordinates_;
        vector<Mat_<double> > bin_output_;
        Mat_<int> pixel_pair_selected_index_;
    public:
        Fern();
        void train(const vector<vector<double> >& pixel_density,
                const Mat_<double>& covariance,
                const Mat_<double>& pixel_coordinates,
                const Mat_<int>& nearest_keypoint_index,
                vector<Mat_<double> >& current_shapes,
                int pixel_pair_num_in_fern,
                vector<Mat_<double> >& normalized_targets,
                vector<Mat_<double> >& prediction);
        void predict(const Mat_<uchar>& image, Mat_<double>& shape, Bbox& bounding_box,const Mat_<double>& mean_shape,
                double scale, const Mat_<double>& rotation, Mat_<double>& prediction);
        void write(ofstream& fout);
        void read(ifstream& fin);

};
class FernCascade{
    private:
        vector<Fern> primary_fern_;
        int second_level_num_;
    public:
        FernCascade();
        void train(const vector<Mat_<uchar> >& images,
                const vector<Mat_<double> >& target_shapes,
                int second_level_num,
                vector<Mat_<double> >& current_shapes,
                int pixel_pair_num,
                vector<Mat_<double> >& normalized_targets,
                int pixel_pair_in_fern,
                const Mat_<double>& mean_shape,
                const vector<Bbox>& target_bounding_box);
        void predict(const Mat_<uchar>& image, Mat_<double>& shape, Bbox& bounding_box,const Mat_<double>& mean_shape);
        void write(ofstream& fout);
        void read(ifstream& fin);        
};
class ShapeRegressor{
    private:
        vector<Mat_<uchar> > images_;
        vector<Mat_<double> > current_shapes_;
        vector<Mat_<double> > target_shapes_;
        vector<FernCascade> fern_cascades_;
        vector<Bbox> bbox_;
        int first_level_num_;
        int second_level_num_;
        int pixel_pair_num_;
        int training_num_; 
        int landmark_num_;
        int img_width_;
        int img_height_;
        int pixel_pair_in_fern_; 
        Mat_<double> mean_shape_;
        void read(ifstream& fin);
        void write(ofstream& fout);
        void calcuate_normalized_matrix(vector<Mat_<double> >&);
        vector<Bbox> target_bounding_box_;
    public:
        ShapeRegressor();
        ShapeRegressor(const Mat_<double>& mean_shape,
                const vector<Mat_<uchar> >& images,
                const vector<Mat_<double> >& target_shapes,
                vector<Mat_<double> >& current_shapes,
                int first_level_num,
                int second_level_num,
                int pixel_pair_num,
                int pixel_pair_in_fern,
                const vector<Bbox>& target_bounding_box);

        void load(const char* file_name);
        void save(const char* file_name);
        void train();
        void predict(const Mat_<uchar>& image, Mat_<double>& shape, Bbox& bounding_box);
        void calcSimil(const Mat_<double> &src,const Mat_<double> &dst,
                double &a,double &b,double &tx,double &ty);
        void invSimil(double a1,double b1,double tx1,double ty1,
                double& a2,double& b2,double& tx2,double& ty2);
};





double calculate_covariance(const vector<double>& v_1, const
        vector<double>& v_2);

void train(const vector<Mat_<uchar> >& input_images,                  
        const vector<Mat_<double> >& target_shapes,
        int initial_number,
        int pixel_pair_num,
        int pixel_pair_in_fern,
        int first_level_num,
        int second_level_num,
        const vector<Bbox>& target_bounding_box);

Mat_<double> test(ShapeRegressor& regressor, const Mat_<uchar>& image, const vector<Mat_<double> > target_shapes,
        Bbox& bounding_box,
        int initial_number,
        const vector<Bbox>& target_bounding_box);

void show_image(const Mat_<uchar>& input_image, const Mat_<double>&  points);

void translate_scale_rotate(const Mat_<double>& shape1, const Mat_<double>& shape2, 
        Mat_<double>& translation, double &scale, Mat_<double>& rotation);

Bbox get_bounding_box(const Mat_<double>& shape);

Mat_<double> shape_normalize(const Mat_<double>& shape, const Bbox& bounding_box);

Mat_<double> reproject_shape_single(const Mat_<double>& shape, const Bbox& bounding_box);

vector<Mat_<double> > reproject_shape(const vector<Mat_<double> >& shapes, const vector<Bbox>& bounding_box);

vector<Mat_<double> > project_shape(const vector<Mat_<double> >& shapes, const vector<Bbox>& bounding_box);

vector<Mat_<double> > compose_shape(const vector<Mat_<double> >& shape1, const vector<Mat_<double> >& shape2, 
        const vector<Bbox>& bounding_box);

vector<Mat_<double> > inverse_shape(const vector<Mat_<double> >& shapes, const vector<Bbox>& bounding_box);

Mat_<double> project_shape(const Mat_<double>& shapes, const Bbox& bounding_box);

Mat_<double>  compose_shape(const Mat_<double>& shape1, const Mat_<double>& shape2, 
        const Bbox& bounding_box);

Mat_<double> reproject_shape(const Mat_<double>& shapes, const Bbox& bounding_box);

vector<Bbox> get_bounding_box(const vector<Mat_<double> >& shapes);

Mat_<double> get_mean_shape(const vector<Mat_<double> >& shapes, const vector<Bbox>& bounding_box);
#endif


