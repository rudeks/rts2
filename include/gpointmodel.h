/*
 * Simplified RTS2 telescope pointing model.
 * Copyright (C) 2015-2016 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __RTS2_RTS2MODEL__
#define __RTS2_RTS2MODEL__

#include "telmodel.h"
#include "teld.h"

namespace rts2telmodel
{

//* maximal number of terms
#define MAX_TERMS   4

typedef enum { GPOINT_SIN=0, GPOINT_COS, GPOINT_TAN, GPOINT_SINCOS, GPOINT_COSCOS, GPOINT_SINSIN, GPOINT_LASTFUN } function_t;
typedef enum { GPOINT_AZ=0, GPOINT_EL, GPOINT_ZD, GPOINT_LASTTERM } terms_t;

/**
 * Extra parameters, currently only for Alt-Az telescopes.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class ExtraParam
{
	public:
		ExtraParam ();
		void parse (char *line);
		double getValue (double az, double el);

		static const char *fns[];
		static const char *pns[];

		function_t function;
		double params[MAX_TERMS];
		terms_t terms[MAX_TERMS];
		double consts[MAX_TERMS];

	private:
		double getParamValue (double az, double alt, int p);
};

/**
 * Telescope pointing model. Based on the following article:
 *
 * ftp://ftp.lowell.edu/pub/buie/idl/pointing/pointing.pdf
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class GPointModel:public TelModel
{
	public:
		GPointModel (double in_latitude);
		virtual ~ GPointModel (void);

		/**
		 * Accepts RTS2_MODEL, as generated by gpoint script.
		 */
		virtual int load (const char *modelFile);

		virtual int apply (struct ln_equ_posn *pos);
		virtual int applyVerbose (struct ln_equ_posn *pos);

		virtual int reverse (struct ln_equ_posn *pos);
		virtual int reverseVerbose (struct ln_equ_posn *pos);
		virtual int reverse (struct ln_equ_posn *pos, double sid);

		/**
		 * Calculate error for alt-az model. Returns expected error in err parameter.
		 */
		void getErrAltAz (struct ln_hrz_posn *hrz, struct ln_hrz_posn *err);

		virtual std::istream & load (std::istream & is);
		virtual std::ostream & print (std::ostream & os);

		double params[9];

		std::list <ExtraParam *> extraParamsAz;
		std::list <ExtraParam *> extraParamsEl;

		bool altaz;
};

std::istream & operator >> (std::istream & is, GPointModel * model);
std::ostream & operator << (std::ostream & os, GPointModel * model);

}
#endif		// !__RTS2_RTS2MODEL__
