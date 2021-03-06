/**
 * @author 
 * @version 2014/02/16
 */

#include "face.h"

void Face::run(){
    // read parameters
    cout<<"Read parameters..."<<endl;
    readParameters();

    // read training data
    cout<<"Read data..."<<endl;
    readData();

    // calculate mean shape;
    cout<<"Get mean shape..."<<endl;

    preprocessing();

    // PROGRAM MODE: 0 for training, 1 for testing
    if(mode == 0){
        calculate_mean_shape();
        double min_x = 1e10;
        double max_x = -1e10;
        for(int i = 0;i < meanShape.size();i++){
            if(meanShape[i].x < min_x){
                min_x = meanShape[i].x;
            }
            if(meanShape[i].x > max_x){
                max_x = meanShape[i].x;
            }
        }

        pixel_range = 0.12 * (max_x - min_x); 
        cout<<pixel_range<<endl;
        cout<<"First level regression..."<<endl;
        firstLevelRegression();
    }
    else if(mode == 1){
        // calculate_mean_shape();
        ifstream fin;
        fin.open("./trainingoutput/meanshape.txt");
        meanShape.clear();
        double x = 0;
        double y = 0;
        while(fin>>x>>y){
            meanShape.push_back(Point2d(x,y));
        }
        fin.close(); 
        faceTest();
    }
}

void Face::readParameters(){
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("./parameters.ini",pt);

    imgNum = pt.get<int>("Training.imgNum");
    featurePixelNum = pt.get<int>("Training.featurePixelNum");
    firstLevelNum = pt.get<int>("Training.firstLevelNum");
    secondLevelNum = pt.get<int>("Training.secondLevelNum");
    keypointNum = pt.get<int>("Training.keypointNum");
    featureNumInFern = pt.get<int>("Training.featureNumInFern");
    shrinkage = pt.get<int>("Training.shrinkage");
    debug = pt.get<int>("Training.debug");
    mode = pt.get<int>("Training.mode");
}


Face::Face(){
    srand(time(NULL));
    averageHeight = 0;
    averageWidth = 0;    
}

void Face::readData(){
    ifstream fin;
    fin.open("./../data/LFPW/keypointsInfor.txt");

    double imageX;
    double imageY;
    double x;
    double y;

    // read image info
    while(fin>>imageX>>imageY>>x>>y){
        imageStartCor.push_back(Point2d(imageX,imageY));
        imgSize.push_back(Point2d(x,y)); 
        vector<Point2d> keypoint;
        for(int i = 0;i < 35;i++){
            fin>>x>>y;
            keypoint.push_back(Point2d(x,y));
        }
        targetShape.push_back(keypoint);
    }

    cout<<targetShape.size()<<endl;

    fin.close();

    // read all image
    Mat img; 
    for(int i = 1;i < imgNum+1;i++){  
        string imgName = "./../data/LFPW/lfpwFaces/";
        imgName = imgName + to_string(i) + ".jpg";
        img = imread(imgName.c_str());
        faceImages.push_back(img);
    }
}


void Face::preprocessing(){
    // first scale all images to the same size
    // change the keypoints coordinates 
    // average all keypoints in the same position

    //get average size
    for(int i = 0;i < imgSize.size();i++){
        averageWidth += imgSize[i].x;
        averageHeight += imgSize[i].y;  
    }    

    averageWidth = averageWidth / imgSize.size();
    averageHeight = averageHeight / imgSize.size();

    if(debug){
        cout<<"Average width and height "<<averageWidth<<" "<<averageHeight<<endl;
    }

    // scale all images
    for(int i = 0;i < faceImages.size();i++){
        resize(faceImages[i],faceImages[i],Size(averageWidth,averageHeight)); 
    }  

    // change the keypoint coordinates
    for(int i = 0;i < targetShape.size();i++){
        for(int j = 0;j < targetShape[i].size();j++){
            double x = targetShape[i][j].x * averageWidth / imgSize[i].x;
            double y = targetShape[i][j].y * averageHeight / imgSize[i].y;  
            targetShape[i][j].x = x;
            targetShape[i][j].y = y;  
        }
    }

    for(int i = 0;i < targetShape.size();i++){
        Point2d temp = get_mean(targetShape[i]);
        target_gravity_center.push_back(temp);

        for(int j = 0;j  < targetShape[i].size();j++){
            targetShape[i][j] = targetShape[i][j] - temp;
        } 
    }

    RNG random_generator(getTickCount());
    for(int i = 0;i < imgNum;i++){
        int index = 0;
        for(int j = 0;j < 20;j++){
            do{
                index = random_generator.uniform(0,imgNum);
            }while(index == i);
            currentShape.push_back(targetShape[index]);
            current_shape_gravity_center.push_back(target_gravity_center[index]);
            augment_target_shapes.push_back(targetShape[i]);
        }
    }

}

void Face::getFeaturePixelLocation(){
    // sample a number of pixels from the face images
    // get their coordinates related to the nearest face keypoints
    // random face pixels selected

    featurePixelCoordinates.clear();
    nearestKeypointIndex.clear(); 
    RNG random_generator(getTickCount());

    for(int i = 0;i < featurePixelNum;i++){
        double x = random_generator.uniform(-pixel_range,pixel_range);
        double y = random_generator.uniform(-pixel_range,pixel_range); 
        featurePixelCoordinates.push_back(Point2d(x,y));
        int index = random_generator.uniform(0,keypointNum); 
        nearestKeypointIndex.push_back(index);
    } 


    // cout<<count<<endl;
    cout<<featurePixelCoordinates.size()<<endl;

    ofstream fout;
    fout.open("./trainingoutput/featurePixelCoordinates.txt", std::ofstream::out|std::ofstream::app); 
    for(int i = 0;i < featurePixelCoordinates.size();i++){
        fout<<featurePixelCoordinates[i]<<" "; 
    }
    fout<<endl;


    for(int i = 0;i < nearestKeypointIndex.size();i++){
        fout<<nearestKeypointIndex[i]<<" ";
    }
    fout<<endl;
    fout.close();
}


void Face::extractFeature(const Mat& covariance,const vector<vector<double> >& pixelDensity,
        vector<Point2i>& selectedFeatureIndex,
        vector<double>& threhold){

    // deltaShape: difference between current shape and target shape
    // we put x and y coordinates together in one vectors
    vector<vector<double> > deltaShape; 
    // getDeltaShape(deltaShape);
    for(int i = 0;i < normalize_targets.size();i++){ 
        vector<double> temp;
        for(int j = 0;j < normalize_targets[i].size();j++){
            temp.push_back(normalize_targets[i][j].x);
            temp.push_back(normalize_targets[i][j].y); 
        } 
        deltaShape.push_back(temp);
    } 

    selectedFeatureIndex.clear();
    threhold.clear();

    //get a random direction
    for(int i = 0;i < featureNumInFern;i++){    

        // get a random direction, so that we can project the deltaShape to 
        // this direction
        vector<double> randomDirection;
        getRandomDirection(randomDirection);


        // calculate the product
        vector<double> projectResult;
        for(int j = 0;j < deltaShape.size();j++){
            double temp = product(deltaShape[j],randomDirection);
            // cout<<"Product result:"<<temp<<endl;
            projectResult.push_back(temp);
        }

        // calculate cov(y,f_i)
        vector<double> covarianceYF;
        for(int j = 0;j < pixelDensity.size();j++){
            double temp = getCovariance(projectResult,pixelDensity[j]);
            // covarianceYF.push_back(getCovariance(projectResult,pixelDensity[j]));      
            // cout<<temp<<" ";
            covarianceYF.push_back(temp);
        }

        // get the pair with highest correlation corr(y,fi - fj);  
        int selectedIndex1 = 0;
        int selectedIndex2 = 0;
        double highest = MINNUM;
        for(int j = 0;j < featurePixelNum;j++){
            for(int k = j+1;k < featurePixelNum;k++){

                // selected feature index should be ignored
                bool flag = false;
                for(int p = 0;p < selectedFeatureIndex.size();p++){
                    if(j == selectedFeatureIndex[p].x && k == selectedFeatureIndex[p].y){
                        flag = true;
                        break;
                    }
                    if(k == selectedFeatureIndex[p].x && j == selectedFeatureIndex[p].y){
                        flag = true;
                        break;
                    }
                }
                if(flag)
                    continue;


                double temp1 = covarianceYF[j];
                double temp2 = covarianceYF[k];
                double temp3 = covariance.at<double>(k,j);

                if(abs(temp3) < 1e-20){
                    // cout<<temp3<<endl;
                    // cout<<"Denominator cannot be zero."<<endl;
                    // exit(-1); 
                    continue;
                }

                double temp4 = (temp1 - temp2) / temp3;

                if(abs(temp4) > highest){
                    highest = abs(temp4);

                    selectedIndex1 = j;
                    selectedIndex2 = k;
                }
            }
        }


        selectedFeatureIndex.push_back(Point2i(selectedIndex1,selectedIndex2));

        //vector<double> densityDiff;
        double max_diff = -1; 
        for(int j = 0;j < pixelDensity[0].size();j++){
            double temp1 = pixelDensity[selectedIndex1][j];
            double temp2 = pixelDensity[selectedIndex2][j];
            // densityDiff.push_back(temp1 - temp2);
            if(abs(temp1-temp2) > max_diff){
                max_diff = abs(temp1-temp2);
            }
        } 

        double best_thresh = -1;
        double best_var = -1;
        RNG rg(getTickCount());
        
        best_thresh = rg.uniform(-0.2 * max_diff , 0.2 * max_diff);
        
        /*
        for(int j = 0;j < 100;j++){
            double thresh = densityDiff[rg.uniform(0,densityDiff.size())]; 
            int n1 = 0;
            int n2 = 0;
            double m1 = 0;
            double m2 = 0;

            for(int k = 0;k < densityDiff.size();k++){
                if(densityDiff[k] >= thresh){
                    m1 = m1 + projectResult[k];
                    n1++; 
                }
                else{
                    m2 = m2 + projectResult[k];
                    n2++;
                }
            } 

            m1 = m1 / n1;
            m2 = m2 / n2;

            double v1 = 0;
            double v2 = 0;

            for(int k = 0;k < densityDiff.size();k++){
                if(densityDiff[k] >= thresh){
                    v1 = v1 + pow(projectResult[k] - m1, 2.0);
                }
                else{
                    v2 = v2 + pow(projectResult[k]- m2, 2.0);
                }
            }

            double v = n1 * log(v1/n1+1e-6) + n2 * log(v2/n2+1e-6);

            if(best_var < 0 || best_var > v){
                best_var = v;
                best_thresh = thresh;
            }
        }
        */

        threhold.push_back(best_thresh); 
    } 



}

double Face::product(const vector<double>& v1, const vector<double>& v2){
    if(v1.size() != v2.size()){
        cout<<"Input vectors have to be of the same size."<<endl;
        exit(-1);
    }

    double result = 0;
    for(int i = 0;i < v1.size();i++){
        result = result + v1[i] * v2[i]; 
    }

    return result;
}


void Face::getDeltaShape(vector<vector<double> >& deltaShape){
    //calculate the difference between current shape and target shape
    deltaShape.clear();
    for(int i = 0;i < currentShape.size();i++){
        vector<double> difference;
        for(int j = 0;j < currentShape[i].size();j++){
            // Point2d delta = currentShape[i][j] - targetShape[i][j];
            Point2d delta = targetShape[i][j] - currentShape[i][j]; 
            difference.push_back(delta.x);
            difference.push_back(delta.y);
        }
        deltaShape.push_back(difference);
    }
}

void Face::getRandomDirection(vector<double>& randomDirection){
    double sum = 0;
    // the size of randomDirection has to be 2*keypointNum, because we 
    // have put x and y together
    RNG random_generator(getTickCount());
    for(int i = 0;i < 2 * keypointNum;i++){
        // int temp = rand()%1000 + 1; 
        // double temp = random_generator.uniform(-1.1,1.1);
        double temp = random_generator.gaussian(1);
        randomDirection.push_back(temp);
        sum = sum + temp * temp;
    }

    sum = sqrt(sum);

    // normalize randomDirection
    for(int i = 0;i < randomDirection.size();i++){
        randomDirection[i] = randomDirection[i] / sum;  
    }
}


void Face::firstLevelRegression(){

    // first initial currentShape
    // RNG random_generator(getTickCount());
    // for(int i = 0;i < targetShape.size();i++){
    // int index = random_generator.uniform(0,imgNum);
    // currentShape.push_back(targetShape[index]);
    // }
    for(int i = 0;i < currentShape.size();i++){
        normalize_targets.push_back(vectorMinus(augment_target_shapes[i],currentShape[i]));
    }    


    for(int i = 0;i < firstLevelNum;i++){
        cout<<endl;
        cout<<"First level regression: "<<i<<endl;
        cout<<endl;

        current_shape_similar_transform.clear(); 
        // target_similar_transform.clear();
        // normalize targets
        for(int j = 0;j < currentShape.size();j++){ 
            SimilarTransform transform;
            align(currentShape[i], meanShape, transform);
            current_shape_similar_transform.push_back(transform);
        }

        // normalize_targets.clear();

        // for(int i = 0;i < currentShape.size();i++){
        // normalize_targets.push_back(vectorMinus(targetShape[i],currentShape[i]));
        // }


        // for(int j = 0;j < normalize_targets.size();j++){
        // SimilarTransform transform;
        // align(normalize_targets[j],meanShape,transform);
        // target_similar_transform.push_back(transform); 
        // }

        if(i == 0){
            for(int j = 0;j < normalize_targets.size();j++){
                apply_similar_transform(normalize_targets[j],current_shape_similar_transform[j]);
            }                        
        }

        getFeaturePixelLocation();

        currentFileName = "./trainingoutput/" + to_string(i) + ".txt";

        // get the feature pixel location based on currentShape             
        vector<vector<Point2d> > currentFeatureLocation;
        vector<vector<double> > pixelDensity;

        // for each pixel selected in our feature, we have to determine their
        // new coordinates relative to the new keypoint coordinates 
        for(int j = 0;j < featurePixelCoordinates.size();j++){
            vector<Point2d> newCoordinates;
            vector<double> newPixelDensity;
            for(int k = 0;k < currentShape.size();k++){
                Point2d currLocation;
                // currLocation = featurePixelCoordinates[j] + currentShape[k][nearestKeypointIndex[j]];
                currLocation = apply_similar_transform_point(featurePixelCoordinates[j],current_shape_similar_transform[k].inverse());
                currLocation = currLocation + currentShape[k][nearestKeypointIndex[j]]  + current_shape_gravity_center[k];

                newCoordinates.push_back(currLocation);


                // to deal with the cases that may happen: during the regression
                // process, the keypoint coordinates may exceed the range of the
                // image
                if(currLocation.y > averageHeight-1)
                    currLocation.y = averageHeight-1;
                if(currLocation.x > averageWidth-1) 
                    currLocation.x = averageWidth-1;
                if(currLocation.y < 0)
                    currLocation.y = 0;
                if(currLocation.x < 0)
                    currLocation.x = 0;


                Vec3b color = faceImages[k/20].at<Vec3b>((int)(currLocation.y),(int)(currLocation.x));
                int b = color.val[0];
                int g = color.val[1];
                int r = color.val[2];

                // change to grayscale
                double density = 0.2126 * r +  0.7152 * g + 0.0722 * b;
                newPixelDensity.push_back(density);
            }
            currentFeatureLocation.push_back(newCoordinates);
            pixelDensity.push_back(newPixelDensity);
        }     

        // select feature

        // calculate the covariance of f_i and f_j
        // zero_matrix<double> covariance(featurePixelNum,featurePixelNum);
        Mat covariance = Mat::zeros(featurePixelNum,featurePixelNum,CV_64F);

        for(int j = 0;j < pixelDensity.size();j++){
            for(int k = j+1;k < pixelDensity.size();k++){
                double temp1 = getCovariance(pixelDensity[j],pixelDensity[j]);
                double temp2 = getCovariance(pixelDensity[k],pixelDensity[k]);
                double temp3 = 2 * getCovariance(pixelDensity[j],pixelDensity[k]);
                double temp = sqrt(temp1 + temp2 - temp3);

                if(abs(temp) == 0){
                    cout<<pixelDensity[j][0]<<" "<<pixelDensity[j][1]<<endl; 
                    cout<<pixelDensity[k][0]<<" "<<pixelDensity[k][1]<<endl; 
                }
                covariance.at<double>(j,k) = temp;
                covariance.at<double>(k,j) = temp;
            }
        }
        secondLevelRegression(covariance,pixelDensity);    
    }


}

void Face::secondLevelRegression(const Mat& covariance,const vector<vector<double> >& pixelDensity){
    for(int i = 0;i < secondLevelNum;i++){
        // select best feature
        // selectedFeatureIndex records the feture pairs we select, 
        cout<<"Construct fern "<<i<<"...."<<endl;
        vector<Point2i> selectedFeatureIndex;  
        vector<double> thresh;
        extractFeature(covariance,pixelDensity,selectedFeatureIndex,thresh); 

        // record selected feature index 
        ofstream fout;
        fout.open(currentFileName,std::ofstream::out|std::ofstream::app);

        for(int j = 0;j < selectedFeatureIndex.size();j++){
            fout<<selectedFeatureIndex[j].x<<" "<< selectedFeatureIndex[j].y<<" ";
        }
        fout<<endl;
        fout.close();

        //construct a fern using selected best features 
        // cout<<"construct fern"<<endl;
        constructFern(selectedFeatureIndex,pixelDensity,thresh); 
    }   
}

void Face::getRandomThrehold(vector<int>& threhold){
    threhold.clear();   
    int binNum = pow(2.0, featureNumInFern); 
    threhold.push_back(0);
    for(int i = 0;i < binNum;i++){
        // int temp = rand() % 33;
        int temp = i+1;
        threhold.push_back(temp);
    }
    sort(threhold.begin(), threhold.end());

    threhold[binNum] = binNum;    
}




void Face::constructFern(const vector<Point2i>& selectedFeatureIndex,
        const vector<vector<double> >& pixelDensity,
        const vector<double>& thresh){
    // turn each shape into a scalar according to relative intensity of pixels
    // generate random threholds
    // divide shapes into bins based on threholds and scalars
    // for each bin, calculate its output

    // fern result records the bins the image is in
    vector<int> fernResult;

    // each fern corresponds to an output, that is, the amount of incremental of
    // shapes
    vector<vector<Point2d> > fernOutput;
    int binNum = pow(2.0,featureNumInFern);

    // bins record the index of each training faces that belong to this bin
    vector<vector<int> > bins;

    // initialzie
    for(int i = 0;i < binNum;i++){
        vector<int> temp;
        bins.push_back(temp);
    }

    // vector<int> threhold;
    // getRandomThrehold(threhold);

    ofstream fout;
    fout.open(currentFileName,std::ofstream::out | std::ofstream::app);




    for(int i = 0;i < thresh.size();i++){
        fout<<thresh[i]<<" ";
    }
    fout<<endl;


    for(int i = 0;i < currentShape.size();i++){
        int tempResult = 0;
        for(int j = 0;j < selectedFeatureIndex.size();j++){

            double density1 = pixelDensity[selectedFeatureIndex[j].x][i];
            double density2 = pixelDensity[selectedFeatureIndex[j].y][i];

            // binary number: 0 or 1
            // turn binary number into an integer
            if(density1 - density2 >= thresh[j]){
                tempResult = tempResult + int(pow(2.0,j)); 
            }
        }

        bins[tempResult].push_back(i);    
        fernResult.push_back(tempResult);

    }

    // get random threhold, the number of bins is 2^featureNumInFern; 
    // here I haven't used random threhold

    // get output
    for(int i = 0;i < bins.size();i++){
        vector<Point2d> currFernOutput;
        currFernOutput.clear();

        // if no training face in this bin, output zero
        if(bins[i].size() == 0){
            for(int j = 0;j < keypointNum;j++){
                currFernOutput.push_back(Point2d(0,0));
            } 
            fernOutput.push_back(currFernOutput);
            continue;
        }

        for(int j = 0;j < bins[i].size();j++){
            int shapeIndex = bins[i][j];
            if(j == 0){
                // currFernOutput = vectorMinus(targetShape[shapeIndex], currentShape[shapeIndex]);
                currFernOutput = normalize_targets[shapeIndex];
            }
            else{
                // currFernOutput = vectorPlus(currFernOutput, vectorMinus(targetShape[shapeIndex],currentShape[shapeIndex]));
                currFernOutput = vectorPlus(currFernOutput,normalize_targets[shapeIndex]); 
            }
        }

        for(int j = 0;j < currFernOutput.size();j++){
            double temp = 1.0/((1 + shrinkage/bins[i].size()) * bins[i].size());
            currFernOutput[j] = temp * currFernOutput[j]; 
        }

        fernOutput.push_back(currFernOutput);
    }

    // record the fern output 
    // ofstream fout;
    // fout.open(currentFileName,std::ofstream::out | std::ofstream::app);
    for(int i = 0;i < fernOutput.size();i++){
        for(int j = 0;j < fernOutput[i].size();j++){
            fout<<fernOutput[i][j]<<" "; 
        }
        fout<<endl;
    }
    fout.close();

    // update current shape, add the corresponding fern output
    for(int i = 0;i < currentShape.size();i++){
        int binIndex = fernResult[i];

        vector<Point2d> temp = fernOutput[binIndex];
        normalize_targets[i] = vectorMinus(normalize_targets[i],temp); 
        apply_similar_transform(temp,current_shape_similar_transform[i].inverse());
        currentShape[i] = vectorPlus(currentShape[i],temp);
        // normalize_targets[i] = vectorMinus(normalize_targets[i],temp); 
        // currentShape[i] = vectorPlus(currentShape[i],fernOutput[binIndex]);
        // there exists cases that after update, the new keypoint coordinates
        // exceed the range of image, I am not quite sure about how to deal with
        // this image  
        /*
           for(int j = 0;j < currentShape[i].size();j++){
           if(currentShape[i][j].x > averageWidth-1){
           currentShape[i][j].x = averageWidth-1;
           }
           if(currentShape[i][j].y > averageHeight-1){
           currentShape[i][j].y = averageHeight-1;
           }
           if(currentShape[i][j].x < 0){
           currentShape[i][j].x = 0;
           } 
           if(currentShape[i][j].y < 0){
           currentShape[i][j].y = 0;
           }
           } 
           */
    }
}


double Face::getCovariance(const vector<double>& v1, const vector<double>& v2){
    // double expV1 = accumulate(v1.begin(),v1.end(),0);
    // double expV2 = accumulate(v2.begin(),v2.end(),0);
    if(v1.size() != v2.size()){
        cout<<"Input vectors have to be of equal size."<<endl;
        exit(-1);
    }

    double expV1 = 0;
    double expV2 = 0;

    for(int i = 0;i < v1.size();i++){
        expV1 = expV1 + v1[i];
    }

    for(int i = 0;i < v2.size();i++){
        expV2 = expV2 + v2[i];
    }

    expV1 = expV1 / v1.size();
    expV2 = expV2 / v2.size();

    double total = 0;
    for(int i = 0;i < v1.size();i++){
        total = total + (v1[i] - expV1) * (v2[i] - expV2); 
    }
    return total / v1.size();
}

vector<Point2d> Face::vectorMinus(const vector<Point2d>& shape1, const vector<Point2d>& shape2){
    if(shape1.size() != shape2.size()){
        cout<<"Input vectors have to be of the same size."<<endl;
        exit(-1);
    }

    vector<Point2d> result;
    for(int i = 0;i < shape1.size();i++){
        result.push_back(shape1[i] - shape2[i]);
    }
    return result;
}

vector<Point2d> Face::vectorPlus(const vector<Point2d>& shape1, const vector<Point2d>& shape2){
    if(shape1.size() != shape2.size()){
        cout<<"Input vectors have to be of the same size."<<endl;
        exit(-1);
    }

    vector<Point2d> result;
    for(int i = 0;i < shape1.size();i++){
        result.push_back(shape1[i] + shape2[i]);
    }
    return result;
}

void Face::faceTest(){
    ifstream fin;
    fin.open("./trainingoutput/featurePixelCoordinates.txt");

    string testImageName = "./test3.jpg";
    Mat testImg = imread(testImageName.c_str());


    Mat testImg2 = imread("./10.jpg");    


    // int testIndex = 491;
    // Mat testImg = faceImages[testIndex];

    resize(testImg,testImg,Size(averageWidth,averageHeight)); 

    // vector<Point2d> inputPixelCoordinates;
    // vector<int> inputNearestIndex;
    double x = 0;
    double y = 0;
    char temp;

    // vector<Point2d> testCurrentShape = meanShape ;  
    vector<Point2d> testCurrentShape = targetShape[100];

    Point2d mean_point = target_gravity_center[100];

    Mat testImg3 = testImg.clone(); 
    for(int i = 0;i < testCurrentShape.size();i++){
        circle(testImg3,testCurrentShape[i] + mean_point,3,Scalar(255,0,0), -1, 8,0); 
    }
    imshow("output",testImg3);
    waitKey(0);




    for(int i = 0;i < firstLevelNum;i++){
        SimilarTransform transform;
        align(testCurrentShape,meanShape,transform);
        // apply_similar_transform(testCurrentShape,transform);


        cout<<"Level: "<<i<<endl;
        vector<Point2d> inputPixelCoordinates;
        vector<int> inputNearestIndex;
        for(int i = 0;i < featurePixelNum;i++){
            fin>>temp>>x>>temp>>y>>temp;
            inputPixelCoordinates.push_back(Point2d(x,y));
        }

        for(int i = 0;i < featurePixelNum;i++){
            fin>>x;
            inputNearestIndex.push_back(int(x)); 
        }
        secondLevelTest(i,testCurrentShape,inputPixelCoordinates, inputNearestIndex, testImg, mean_point, transform);
    }

    fin.close();

    Mat testImg1 = testImg.clone();
    for(int i = 0;i < meanShape.size();i++){
        circle(testImg1,meanShape[i] + Point2d(100,100),3,Scalar(0,0,255),-1,8,0);
    }
    imshow("initial",testImg1);



    SimilarTransform transform;
    align(targetShape[46], meanShape, transform);

    cout<<transform.a<<endl;
    cout<<transform.b<<endl;





    for(int i = 0;i < testCurrentShape.size();i++){
        circle(testImg,testCurrentShape[i] + mean_point,3,Scalar(255,0,0), -1, 8,0); 
    }
    imshow("output",testImg);
    waitKey(0);
}

void Face::secondLevelTest(int currLevelNum, vector<Point2d>& testCurrentShape, 
        const vector<Point2d>& inputPixelCoordinates,const vector<int>& inputNearestIndex,
        const Mat& testImg, const Point2d& mean_point, SimilarTransform& transform){
    ifstream fin;
    string fileName = "./trainingoutput/" + to_string(currLevelNum) + ".txt"; 
    fin.open(fileName);

    vector<double> pixelDensity;

    for(int i = 0;i < inputPixelCoordinates.size();i++){
        Point2d temp;
        // temp = inputPixelCoordinates[i] + testCurrentShape[inputNearestIndex[i]];    
        temp = apply_similar_transform_point(inputPixelCoordinates[i], transform.inverse());
        temp = temp + testCurrentShape[inputNearestIndex[i]] + mean_point;
        if(temp.y > averageHeight-1)
            temp.y = averageHeight-1;
        if(temp.x > averageWidth-1) 
            temp.x = averageWidth-1;
        if(temp.y < 0)
            temp.y = 0;
        if(temp.x < 0)
            temp.x = 0;
        Vec3b color = testImg.at<Vec3b>((int)(temp.y),(int)(temp.x));
        int r = color.val[2];
        int g = color.val[1];
        int b = color.val[0];
        double density = 0.2126 * r +  0.7152 * g + 0.0722 * b;
        pixelDensity.push_back(density);
    } 



    for(int i = 0;i < secondLevelNum;i++){

        vector<Point2i> selectedFeatureIndex;
        for(int j = 0;j < featureNumInFern;j++){
            double x = 0;
            double y = 0;
            fin>>x>>y;
            selectedFeatureIndex.push_back(Point2i(x,y));   
        } 

        // vector<int> threhold;
        int binNum = pow(2.0, featureNumInFern);

        // for(int i = 0;i < binNum + 1;i++){
        // int temp;
        // fin>>temp;
        // threhold.push_back(temp);
        // }
        vector<double> thresh;
        for(int j = 0;j < featureNumInFern;j++){
            double x = 0;
            fin>>x;
            thresh.push_back(x);
        }


        vector<vector<Point2d> > fernOutput;
        for(int j = 0;j < binNum;j++){
            vector<Point2d> currFernOutput;
            for(int k = 0;k < keypointNum;k++){
                double x = 0;
                double y = 0;
                char temp;
                fin>>temp>>x>>temp>>y>>temp;
                // cout<<x<<" "<<y<<endl;
                currFernOutput.push_back(Point2d(x,y)); 
            } 
            fernOutput.push_back(currFernOutput); 
        }


        int binIndex = 0;
        for(int j = 0;j < featureNumInFern;j++){
            int selectedIndex1 = selectedFeatureIndex[j].x;
            int selectedIndex2 = selectedFeatureIndex[j].y; 
            if(pixelDensity[selectedIndex1] - pixelDensity[selectedIndex2] >= thresh[j]){
                binIndex = binIndex + (int)(pow(2.0,j)); 
            }  
        }

        // for(int j = 0;j < binNum;j++){
        // if(binIndex >= threhold[j] && binIndex < threhold[j+1]){
        // binIndex = j;
        // break;
        // }
        // }




        // Mat tempImg = testImg.clone();
        vector<Point2d> temp = fernOutput[binIndex];

        apply_similar_transform(temp,transform.inverse());




        testCurrentShape = vectorPlus(testCurrentShape,temp);  
        // for(int i = 0;i < testCurrentShape.size();i++){
        // circle(tempImg,testCurrentShape[i],3,Scalar(255,0,0), -1, 8,0); 
        // }
        // imshow("test",tempImg);
        // waitKey(1);
        // Mat testImg1 = testImg.clone(); 
        // for(int i = 0;i < testCurrentShape.size();i++){
        // circle(testImg1,testCurrentShape[i] + mean_point,3,Scalar(255,0,0), -1, 8,0); 
        // }
        // imshow("output",testImg1);
        // waitKey(1);

        /*    for(int j = 0;j < testCurrentShape.size();j++){ */
        // if(testCurrentShape[j].x > averageWidth-1){
        // cout<<"Extend..."<<endl;
        // testCurrentShape[j].x = averageWidth-1;
        // }
        // if(testCurrentShape[j].y > averageHeight-1){
        // cout<<"Extend..."<<endl;
        // testCurrentShape[j].y = averageHeight-1;
        // }
        // if(testCurrentShape[j].x < 0){
        // cout<<"Extend..."<<endl;
        // testCurrentShape[j].x = 0;
        // } 
        // if(testCurrentShape[j].y < 0){
        // cout<<"Extend..."<<endl;
        // testCurrentShape[j].y = 0;
        // }
        /* } */
    }
    fin.close();

}



void Face::calculate_mean_shape(){
    // center each shape at origin
    vector<vector<Point2d> > input_shapes = currentShape;

    cout<<"Calculate mean shape..."<<endl;
    for(int i = 0;i < input_shapes.size();i++){
        // double mean_x = 0;
        // double mean_y = 0; 
        // for(int j = 0;j < input_shapes[i].size();j++){
        // mean_x += input_shapes[i][j].x;
        // mean_y += input_shapes[i][j].y;     
        // }
        // mean_x = mean_x / input_shapes[i].size();
        // mean_y = mean_y / input_shapes[i].size();
        Point2d temp = get_mean(input_shapes[i]);

        for(int j = 0;j < input_shapes[i].size();j++){
            input_shapes[i][j].x -= temp.x;
            input_shapes[i][j].y -= temp.y;
        }
    }

    // get the mean shape
    vector<Point2d> new_mean_shape;
    vector<Point2d> current_mean_shape;
    // scale_shape(input_shapes[0]);
    new_mean_shape = input_shapes[0];
    vector<Point2d> x0 = new_mean_shape; 
    do{
        cout<<"iteration..."<<endl;
        current_mean_shape = new_mean_shape;
        for(int i = 0;i < new_mean_shape.size();i++){
            new_mean_shape[i].x = 0;
            new_mean_shape[i].y = 0;
        }
        for(int i = 0;i < input_shapes.size();i++){
            SimilarTransform transform;
            align(input_shapes[i], current_mean_shape,transform);
            apply_similar_transform(input_shapes[i],transform);     
            new_mean_shape = vectorPlus(new_mean_shape,input_shapes[i]);
        } 
        for(int i = 0;i < new_mean_shape.size();i++){
            new_mean_shape[i] = (1.0/input_shapes.size()) * new_mean_shape[i];
        }
        SimilarTransform transform;
        align(new_mean_shape,x0,transform);
        apply_similar_transform(new_mean_shape,transform);
        // scale_shape(new_mean_shape);
    }while(cal_vector_norm(vectorMinus(current_mean_shape,new_mean_shape)) > 1e-10);

    meanShape = current_mean_shape;

    cout<<"Finish calculate meanshape..."<<endl;
    ofstream fout;
    fout.open("./trainingoutput/meanshape.txt");
    for(int i = 0;i < meanShape.size();i++){
        fout<<meanShape[i].x <<"    "<<meanShape[i].y<<endl; 
    }
    fout.close();
}

// scale each shape x such that ||x|| = 1
void Face::scale_shape(vector<Point2d>& input_shape){
    double sum = 0;
    for(int i = 0;i < input_shape.size();i++){
        sum += (pow(input_shape[i].x,2.0) + pow(input_shape[i].y,2.0));
    }
    sum = sqrt(sum);
    for(int i = 0;i < input_shape.size();i++){
        input_shape[i].x /= sum;
        input_shape[i].y /= sum; 
    }
}

double Face::cal_vector_norm(const vector<Point2d>& input_vector){
    double sum = 0;
    for(int i = 0;i < input_vector.size();i++){
        sum += pow(input_vector[i].x,2.0);
        sum += pow(input_vector[i].y,2.0); 
    }
    return sqrt(sum);
}

void Face::align(const vector<Point2d>& src, const vector<Point2d>& dst, SimilarTransform& transform){
    double scale = 0;
    double theta = 0;
    double a = 0;
    double b = 0;
    double part1 = 0;
    double part2 = 0;
    for(int i = 0;i < src.size();i++){
        part1 += (src[i].x * dst[i].x);
        part1 += (src[i].y * dst[i].y);
        part2 += (src[i].x * dst[i].y - src[i].y * dst[i].x);
    } 
    double part3 = pow(cal_vector_norm(src),2.0);

    a = part1 / part3;
    b = part2 / part3;

    transform.a = a;
    transform.b = b;

    // transform.a = 1;
    // transform.b = 0; 

}

void Face:: apply_similar_transform(vector<Point2d>& src, const SimilarTransform& transform){
    // double cos_theta = cos(transform.theta);
    // double sin_theta = sin(transform.theta);
    double a = transform.a;
    double b = transform.b;
    for(int i = 0;i < src.size();i++){
        double x = src[i].x;
        double y = src[i].y;
        src[i].x = a * x - b * y;
        src[i].y = b * x + a * y; 
    }
}

Point2d Face:: apply_similar_transform_point(const Point2d& point, const SimilarTransform& transform){
    Point2d result(0,0);
    result.x = point.x * transform.a - point.y * transform.b;
    result.y = point.x * transform.b + point.y * transform.a;

    return result;
}


Point2d Face::get_mean(const vector<Point2d>& point){
    Point2d result(0,0);
    for(int i = 0;i < point.size();i++){
        result.x += point[i].x;
        result.y += point[i].y; 
    } 
    result.x /= point.size();
    result.y /= point.size();
    // cout<<result.x<<endl;
    return result;
}



SimilarTransform:: SimilarTransform(){
    a = 0;
    b = 0;
}
SimilarTransform SimilarTransform:: inverse(){
    SimilarTransform transform;
    double temp = a * a + b * b;
    transform.a = a / temp;
    transform.b = -b / temp;
    return transform; 
}






