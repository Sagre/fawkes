
/***************************************************************************
 *  field_pointer.h - BlackBoard Interface Field Pointer
 *
 *  Created: Mon Oct 09 18:34:11 2006
 *  Copyright  2006-2008  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#ifndef _INTERFACE_FIELD_POINTER_H_
#define _INTERFACE_FIELD_POINTER_H_

#include <interface/interface.h>
#include <interface/types.h>

namespace fawkes {

/** Direct pointer to an interface field.
 * This class allows for keeping a pointer to an interface value which is
 * valid for the whole lifetime of the interface.
 * @author Tim Niemueller
 */
template <typename FieldType>
class InterfaceFieldPointer
{
public:
	/** Constructor.
   * @param type value type of the field
   * @param name name of the field
   * @param value pointer to the value of the field
   */
	InterfaceFieldPointer(interface_fieldtype_t type, const char *name, FieldType *value)
	{
		type_  = type;
		name_  = name;
		value_ = value;
	}

	/** Get the type of the field.
   * @return type of the field
   */
	interface_fieldtype_t
	get_type() const
	{
		return type_;
	}

	/** Get name of the field.
   * @return name of the field.
   */
	const char *
	get_name() const
	{
		return name_;
	}

	/** Get current value of the field.
   * @return current vlaue of the field.
   */
	FieldType
	get_value() const
	{
		return *value_;
	}

	/** Set value of the field.
   * @param value new value to set for the field.
   */
	void
	set_value(FieldType value)
	{
		*value_ = value;
	}

private:
	interface_fieldtype_t type_;
	const char *          name_;
	volatile FieldType *  value_;
};

} // end namespace fawkes

#endif
