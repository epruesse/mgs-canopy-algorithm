#include <algorithm>

#include <omp.h>

#include <boost/foreach.hpp>


#include <CanopyClustering.hpp>
#include <Log.hpp>

Canopy* CanopyClusteringAlg::create_canopy(Point* origin, vector<Point*>& points, vector<Point*>& close_points, double max_neighbour_dist, double max_close_dist, bool set_close_points){

    std::vector<Point*> neighbours;

    if(set_close_points){
        //Go through all points and set the close points to contain the ones that are "close"
        close_points.clear();//Will not reallocate
        close_points.push_back(origin);
        for(int i=0; i<points.size(); i++){

            Point* potential_neighbour = points[i];
            double dist = get_distance_between_points(origin, potential_neighbour);

            if(dist < max_close_dist){

                close_points.push_back(potential_neighbour);

                if(dist < max_neighbour_dist){

                    neighbours.push_back(potential_neighbour);

                }
            } 
        }
    } else {

        for(int i=0; i<close_points.size(); i++){

            Point* potential_neighbour = close_points[i];
            double dist = get_distance_between_points(origin, potential_neighbour);

            //TODO: get rid of this hack
            if((dist < max_neighbour_dist) && (!potential_neighbour->id.compare("!GENERATED!"))){
                neighbours.push_back(potential_neighbour);
            }
        }

    }
    //_log(logINFO) << "Close points size: " << close_points.size() << " neighbours: " << neighbours.size();

    neighbours.push_back(origin);

    Point* center;

    if(neighbours.size() == 1)
        center = origin;
    else
        center = get_centroid_of_points(neighbours);

    return new Canopy(origin, center, neighbours);

}

void CanopyClusteringAlg::filter_clusters_by_single_point_skew(double max_single_data_point_proportion, std::vector<Canopy*>& canopies_to_filter){

    vector<int> canopy_indexes_to_remove;

    for(int i=0; i < canopies_to_filter.size(); i++){
        Point* ccenter = canopies_to_filter[i]->center;
        if(! ccenter->check_if_single_point_proportion_is_smaller_than(max_single_data_point_proportion) )
            canopy_indexes_to_remove.push_back(i);
    }

    std::sort(canopy_indexes_to_remove.begin(), canopy_indexes_to_remove.end());
    std::reverse(canopy_indexes_to_remove.begin(), canopy_indexes_to_remove.end());

    for(int i=0; i < canopy_indexes_to_remove.size(); i++)
        canopies_to_filter.erase(canopies_to_filter.begin() + canopy_indexes_to_remove[i]);

}

void CanopyClusteringAlg::filter_clusters_by_zero_medians(int min_num_non_zero_medians, std::vector<Canopy*>& canopies_to_filter){

    vector<int> canopy_indexes_to_remove;

    for(int i=0; i < canopies_to_filter.size(); i++){
        Point* ccenter = canopies_to_filter[i]->center;
        if(! ccenter->check_if_num_non_zero_samples_is_greater_than_x(min_num_non_zero_medians) )
            canopy_indexes_to_remove.push_back(i);
    }

    std::sort(canopy_indexes_to_remove.begin(), canopy_indexes_to_remove.end());
    std::reverse(canopy_indexes_to_remove.begin(), canopy_indexes_to_remove.end());

    for(int i=0; i < canopy_indexes_to_remove.size(); i++)
        canopies_to_filter.erase(canopies_to_filter.begin() + canopy_indexes_to_remove[i]);

}

std::vector<Canopy*> CanopyClusteringAlg::multi_core_run_clustering_on(vector<Point*>& points, double max_canopy_dist, double max_close_dist, double max_merge_dist, double max_step_dist){

    int num_threads = 16;
    omp_set_num_threads(num_threads);


    _log(logDEBUG1) << "############Creating Canopies############";
    _log(logDEBUG1) << "Parameters:";
    _log(logDEBUG1) << "max_canopy_dist:\t " << max_canopy_dist;
    _log(logDEBUG1) << "max_close_dist:\t " << max_close_dist;
    _log(logDEBUG1) << "max_merge_dist:\t " << max_merge_dist;
    _log(logDEBUG1) << "max_step_dist:\t " << max_step_dist;

    //double max_canopy_dist = 0.1;
    //double max_close_dist = 0.4;
    //double max_merge_dist = 0.03;
    //double max_step_dist = 0.1;

    //TODO: ensure it is actually random
    //std::random_shuffle(points.begin(), points.end());

    //TODO: this may be super slow!
    boost::unordered_set<Point*> marked_points;

    std::vector<Canopy*> canopy_vector;

    vector<Point*> close_points;
    close_points.reserve(points.size());

    //
    //Create canopies
    //
        
    int num_canopy_jumps = 0;


#pragma omp parallel for shared(marked_points, canopy_vector, num_canopy_jumps) firstprivate(close_points, max_canopy_dist, max_close_dist, max_merge_dist, max_step_dist) schedule(dynamic)
    for(int origin_i = 0; origin_i < points.size(); origin_i++){
        Point* origin = points[origin_i]; 

        if(marked_points.find(origin) != marked_points.end())
            continue;

        _log(logINFO) << "Unmarked points count: " << points.size() - marked_points.size() << " Marked points count: " << marked_points.size();
        _log(logINFO) << "points.size: " << points.size() << " origin_i: " << origin_i << " origin->id: " << origin->id ;

        _log(logDEBUG2) << "Current canopy origin: " << origin->id;

        Canopy *c1;
        Canopy *c2;

        c1 = create_canopy(origin, points, close_points, max_canopy_dist, max_close_dist, true);

        c2 = create_canopy(c1->center, points, close_points, max_canopy_dist, max_close_dist, false);

        double dist = get_distance_between_points(c1->center, c2->center);

        _log(logDEBUG3) << *c1;
        _log(logDEBUG3) << *c2;
        _log(logDEBUG3) << "dist: " << dist;

        //cout << "Point1:" << endl;
        //cout << *c1 << endl;
        //cout << "Point2:" << endl;
        //cout << *c2 << endl;
        //cout << "First potential jump correlation: " << correlation << endl;
        

        while(dist > max_step_dist){
            delete c1;
            c1=c2;

#pragma omp atomic
            num_canopy_jumps++;

            c2=create_canopy(c1->center, points, close_points, max_canopy_dist, max_close_dist, false);
            dist = get_distance_between_points(c1->center, c2->center); 
            _log(logDEBUG3) << *c1;
            _log(logDEBUG3) << *c2;
            _log(logDEBUG3) << "distance: " << dist;
        }

        //Now we know that c1 and c2 are close enough and we should choose the one that has more neighbours

        Canopy* final_canopy = c1->neighbours.size() > c2->neighbours.size() ? c1 : c2;

#pragma omp critical
        {
            //Do not commit anything if by chance another thread marked the current origin
            if(marked_points.find(origin) == marked_points.end()){
                marked_points.insert(origin);

                canopy_vector.push_back(final_canopy);

                BOOST_FOREACH(Point* n, c1->neighbours){
                    marked_points.insert(n);
                }
                //TODO: Could be done better
                if(!(final_canopy->origin->id.compare("!GENERATED!"))){
                    marked_points.insert(c1->origin);
                }

            }

        }

    }

    _log(logINFO) << "Avg. number of canopy jumps: " << num_canopy_jumps/(double)canopy_vector.size();
    _log(logINFO) << "Number of canopies before merging: " << canopy_vector.size();

    std::vector<Canopy*> merged_canopy_vector;

    _log(logDEBUG1) << "############Merging canopies############";
    while(canopy_vector.size()){
        bool any_canopy_merged = false;

        std::vector<int> canopies_to_be_merged_index_vector;
        std::vector<Canopy*> canopies_to_merge;

        //This is the canopy we will look for partners for
        Canopy* c = canopy_vector[0];

        //Get indexes of those canopies that are nearby
#pragma omp parallel for shared(canopies_to_be_merged_index_vector) 
        for(int i = 1; i < canopy_vector.size(); i++){

            Canopy* c2 = canopy_vector[i]; 

            _log(logDEBUG3) << "Calculating distances";
            _log(logDEBUG3) << *c->center;
            _log(logDEBUG3) << *c2->center;

            double dist = get_distance_between_points(c->center, c2->center);

            _log(logDEBUG3) << "Distance: " << dist;

            if(dist < max_merge_dist){
#pragma omp critical
                {
                    canopies_to_be_merged_index_vector.push_back(i);
                }
            }

        }

        //Assemble all canopies close enough in one bag 
        if( canopies_to_be_merged_index_vector.size() ){

            //Put all canopies to be merged in one bag
            canopies_to_merge.push_back(canopy_vector[0]);

            BOOST_FOREACH(int i, canopies_to_be_merged_index_vector)
                canopies_to_merge.push_back(canopy_vector[i]);

        }
            

        //Remove canopies that were close enough from the canopy vector
        if( canopies_to_be_merged_index_vector.size() ){
            //Now we delete those canopies that we merged

            //First - sort so that the indexes won't change during our merger
            std::sort(canopies_to_be_merged_index_vector.begin(), canopies_to_be_merged_index_vector.end());

            while(canopies_to_be_merged_index_vector.size()){
                canopy_vector.erase(canopy_vector.begin() + canopies_to_be_merged_index_vector.back());
                canopies_to_be_merged_index_vector.pop_back();
            }
            canopy_vector.erase(canopy_vector.begin());

            
        }

        //Actually merge the canopies and add the new one to the beginning of the canopy vector
        if( canopies_to_merge.size() ){
            any_canopy_merged = true;

            Canopy* merged_canopy= new Canopy();

            BOOST_FOREACH(Canopy* canopy, canopies_to_merge){
               merged_canopy->neighbours.insert(merged_canopy->neighbours.begin(), canopy->neighbours.begin(), canopy->neighbours.end()); 
                //delete those canopies which are merged into one
               delete canopy;
            }

            merged_canopy->center = get_centroid_of_points( merged_canopy->neighbours );

            canopy_vector.insert(canopy_vector.begin(), merged_canopy);

        }

        //If no canopies were merged remove the canopy we compared against the others
        if( !any_canopy_merged ){
            merged_canopy_vector.push_back(canopy_vector[0]);
            canopy_vector.erase(canopy_vector.begin());
        }
    }

    _log(logINFO) << "Number of canopies after merging: " << merged_canopy_vector.size();

    return merged_canopy_vector;

}

