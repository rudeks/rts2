/*
 * Driver for Sitech focuser
 * Copyright (C) 2016 Petr Kubanek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "connection/sitech.h"
#include "focusd.h"
#include "sitech-multidev.h"

namespace rts2focusd
{

class SitechFocuser:public Focusd, public SitechMultidev
{
	public:
		SitechFocuser (const char *dev_name, rts2core::ConnSitech *sitech_c, const char *defaults, const char *extTemp);

	protected:
		virtual int setValue (rts2core::Value *oldValue, rts2core::Value *newValue);

		virtual int info ();

		virtual int commandAuthorized (rts2core::Connection *conn);

		virtual int setTo (double num);
		virtual double tcOffset ();

		virtual bool isAtStartPosition ();

	private:
		rts2core::ValueLong *encoder;

		rts2core::ValueLong *focSpeed;
		rts2core::ValueString *errors;
		rts2core::ValueInteger *errors_val;
		rts2core::ValueBool *autoMode;

		int last_errors;
};

}
