/**
 * Metagenomics Canopy Clustering Implementation
 *
 * Copyright (C) 2013, 2014 Piotr Dworzynski (piotr@cbs.dtu.dk), Technical University of Denmark
 * Copyright (C) 2015 Enterome
 *
 * This file is part of Metagenomics Canopy Clustering Implementation.
 *
 * Metagenomics Canopy Clustering Implementation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Metagenomics Canopy Clustering Implementation is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <functional>
#include <numeric>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>

#include <Point.hpp>
#include <Log.hpp>
#include <Stats.hpp>

using namespace std;

Point::Point(char* line){
    _log(logDEBUG3)<< "Point constructor, got: \"" << line << "\""; 

	char* line_ptr = line;

    //Read gene id - first word in the line
	while (*line_ptr != '\0' && !std::isspace(*line_ptr))
			line_ptr++;
    id = string(line, line_ptr-line);

    _log(logDEBUG3)<< "Point constructor, point id: \"" << id << "\""; 

    //Fill vector with data samples
    std::vector<float> sample_data_vector;
    sample_data_vector.reserve(700);

    while(*line_ptr != '\0' ){
        sample_data_vector.push_back(strtof(line_ptr,&line_ptr));
    }

    //Get number of samples for this point
    num_data_samples = sample_data_vector.size();
    _log(logDEBUG3)<< "Point constructor, num data samples: \"" << num_data_samples << "\""; 

    //Allocate and copy samples into array
    sample_data = new float[num_data_samples];
    sample_data_pearson_precomputed = NULL; 
    for(size_t i = 0; i < sample_data_vector.size(); i++){
        sample_data[i] = sample_data_vector[i];
    }

}

Point::Point(const Point& p){
    id = p.id;
    num_data_samples = p.num_data_samples;

    sample_data = new float[num_data_samples];
    for(size_t i=0; i < num_data_samples;i++){
        sample_data[i] = p.sample_data[i];
    }

    sample_data_pearson_precomputed = new double[num_data_samples];
    for(size_t i=0; i < num_data_samples;i++){
        sample_data_pearson_precomputed[i] = p.sample_data_pearson_precomputed[i];
    }
}


Point::~Point(){
    delete[] sample_data;
    delete[] sample_data_pearson_precomputed;
}


bool Point::check_if_num_non_zero_samples_is_greater_than_x(int x){
    int num_non_zero_samples = 0;
    for(size_t i=0; i < num_data_samples; i++){
        if(sample_data[i] > 1e-7){
            num_non_zero_samples++;
            if(num_non_zero_samples >= x)
                return true;
        }
    }
    return false;

}

bool Point::check_if_top_three_point_proportion_is_smaller_than(double x){

    vector<double> temp_data_samples(num_data_samples);
	std::copy(sample_data, sample_data + num_data_samples, temp_data_samples.begin());

	std::nth_element(temp_data_samples.begin(), temp_data_samples.begin()+2, temp_data_samples.end(), std::greater<double>());

    double sum_data_samples = std::accumulate(temp_data_samples.begin(), temp_data_samples.end(), 0.0);
    double sum_top_three = std::accumulate(temp_data_samples.begin(), temp_data_samples.begin()+3, 0.0);

    if(sum_data_samples > 1e-10){
        return (sum_top_three / sum_data_samples) < (x - 1e-10);
    } else {
        //All samples have 0 value - can't divide by 0
        return false;
    }

}

bool Point::check_if_single_point_proportion_is_smaller_than(double x){
    double sum_data_samples = 0.0;
    double max_data_sample = 0.0;

    for(size_t i=0; i < num_data_samples; i++){
        if(max_data_sample < sample_data[i])
            max_data_sample = sample_data[i];

        sum_data_samples += sample_data[i];
    }

    return (max_data_sample / sum_data_samples) < x;
}

void verify_proper_point_input_or_die(const std::vector<Point*>& points){
    
    //Verify all points have the same number of samples
    size_t num_samples = points[0]->num_data_samples;
    BOOST_FOREACH(const Point* point, points){
        assert(point->num_data_samples == num_samples);
    }

    _log(logINFO) << "Observed number of samples per point: " << num_samples;
    _log(logINFO) << "Number of points read: " << points.size();

}

double get_distance_between_points(const Point* p1, const Point* p2){

    int len = p1->num_data_samples;
    double dist = 1.0 - fabs(pearsoncorr_from_precomputed(len, p1->sample_data_pearson_precomputed, p2->sample_data_pearson_precomputed));

    //if(log_level >= logDEBUG3){
    //    _log(logDEBUG3) << "<<<<<<DISTANCE<<<<<<";
    //    _log(logDEBUG3) << "point: " << p1->id;
    //    for(int i=0; i < p1->num_data_samples; i++){
    //        _log(logDEBUG3) << "\t"<<p1->sample_data[i];
    //    }
    //    _log(logDEBUG3) << "point: " << p2->id;
    //    for(int i=0; i < p2->num_data_samples; i++){
    //        _log(logDEBUG3) << "\t"<<p2->sample_data[i];
    //    }
    //    _log(logDEBUG3) << "distance: " << dist;
    //}

    return dist; 
}

Point* get_centroid_of_points(const std::vector<Point*>& points){
    assert(points.size());

    Point* centroid = new Point(*(points[0]));
    centroid->id = "!GENERATED!";

   	const size_t num_samples = points[0]->num_data_samples;
	std::vector<float> point_samples(points.size());
	const size_t mid = (point_samples.size() - 1)/2;

    for(size_t i = 0; i < num_samples; i++){
		for (size_t curr_point = 0; curr_point < point_samples.size(); curr_point++)
            point_samples[curr_point] = points[curr_point]->sample_data[i];

        float median;
        if(point_samples.size()%2 == 0){
			std::nth_element(point_samples.begin(), point_samples.begin()+mid+1, point_samples.end());
			median = point_samples[mid+1];
			median += *std::max_element(point_samples.begin(), point_samples.begin()+mid+1);
			median /= 2.0;
        } else {
			std::nth_element(point_samples.begin(), point_samples.begin()+mid, point_samples.end());
            median = point_samples[mid];
        }

        centroid->sample_data[i] = median;
    }

    precompute_pearson_data(centroid->num_data_samples, centroid->sample_data, centroid->sample_data_pearson_precomputed);
    
    return centroid;
}

std::ostream& operator<<(std::ostream& ost, const Point& p)
{
        ost << "============================" << std::endl;
        ost << "Point: " << p.id << std::endl;
        for(size_t i=0; i < p.num_data_samples; i++){
            ost << p.sample_data[i] << "\t" ;
        }
        ost << std::endl;
        ost << "============================" << std::endl;
        
        return ost;
}

