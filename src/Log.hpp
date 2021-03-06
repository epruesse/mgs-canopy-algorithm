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
#ifndef _LOG_HPP_
#define _LOG_HPP_

#include <iostream>
#include <sstream>
#include <vector>

enum loglevel_e
    {logERR, logPROGRESS, logWARN, logINFO, logDEBUG, logDEBUG1, logDEBUG2, logDEBUG3, logDEBUG4};

class Logger
{
public:
	static const std::vector<std::string> valid_verbosities;
	static loglevel_e log_level;

    Logger();
    ~Logger();

    template <typename T> Logger& operator<<(T const & value)
    {
        buffer << value;
        return *this;
    }
	
	static bool set_log_level(const std::string& verbosity_option);



private:
    std::ostringstream buffer;
};


#define _log(level) \
if (level > Logger::log_level) ; \
else Logger()

#endif
