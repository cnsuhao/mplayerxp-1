#ifndef __M_PROPERTY_H_INCLUDED
#define __M_PROPERTY_H_INCLUDED 1

/// \defgroup Properties
///
/// Properties provide an interface to query and set the state of various
/// things in MPlayer. The API is based on the \ref Options API like the
/// \ref Config, but instead of using variables, properties use an ioctl like
/// function. The function is used to perform various actions like get and set
/// (see \ref PropertyActions).
///@{

/// \file

/// \defgroup PropertyActions Property actions
/// \ingroup Properties
///@{

enum {
/** \param arg Pointer to a variable of the right type.
 */
    M_PROPERTY_GET		=0, /// Get the current value.
/** Set the variable to a newly allocated string or NULL.
 *  \param arg Pointer to a char* variable.
 */
    M_PROPERTY_PRINT		=1, /// Get a string representing the current value.
/** The variable is updated to the value actually set.
 *  \param arg Pointer to a variable of the right type.
 */
    M_PROPERTY_SET		=2, /// Set a new value.
    M_PROPERTY_PARSE		=3, /// Set a new value from a string. param arg String containing the value.
/** The sign of the argument is also taken into account if applicable.
 *  \param arg Pointer to a variable of the right type or NULL.
 */
    M_PROPERTY_STEP_UP		=4, /// Increment the current value.
/** The sign of the argument is also taken into account if applicable.
 *  \param arg Pointer to a variable of the right type or NULL.
 */
    M_PROPERTY_STEP_DOWN	=5, /// Decrement the current value.
/** Set the variable to a newly allocated string or NULL.
 *  \param arg Pointer to a char* variable.
 */
    M_PROPERTY_TO_STRING	=6, /// Get a string containg a parsable representation.
    M_PROPERTY_KEY_ACTION	=7, /// Pass down an action to a sub-property.
    M_PROPERTY_GET_TYPE		=8 /// Get a m_option describing the property.
};
///@}

/// \defgroup PropertyActionsArg Property actions argument type
/// \ingroup Properties
/// \brief  Types used as action argument.
///@{

/// Argument for \ref M_PROPERTY_KEY_ACTION
typedef struct {
    const char* key;
    int action;
    const any_t* arg;
} m_property_action_t;

///@}

/// \defgroup PropertyActionsReturn Property actions return code
/// \ingroup Properties
/// \brief  Return values for the control function.
///@{

enum {
    M_PROPERTY_OK		=1, /// Returned on success.
    M_PROPERTY_ERROR		=0, /// Returned on error.
    M_PROPERTY_UNAVAILABLE	=-1, /// \brief Returned when the property can't be used, for example something about the subs while playing audio only
    M_PROPERTY_NOT_IMPLEMENTED	=-2, /// Returned if the requested action is not implemented.
    M_PROPERTY_UNKNOWN		=-3, /// Returned when asking for a property that doesn't exist.
    M_PROPERTY_DISABLED		=-4 /// Returned when the action can't be done (like setting the volume when edl mute).
};
///@}

/// \ingroup Properties
/// \brief Property action callback.
typedef int(*m_property_ctrl_f)(const m_option_t* prop,int action,const any_t* arg,any_t*ctx);

/// Do an action on a property.
/** \param prop_list The list of properties.
 *  \param prop The path of the property.
 *  \param action See \ref PropertyActions.
 *  \param arg Argument, usually a pointer to the data type used by the property.
 *  \return See \ref PropertyActionsReturn.
 */
int m_property_do(m_option_t* prop_list, const char* prop,
		  int action, any_t* arg, any_t*ctx);

/// Print a list of properties.
void m_properties_print_help_list(m_option_t* list);

/// Expand a property string.
/** This function allows to print strings containing property values.
 *  ${NAME} is expanded to the value of property NAME or an empty
 *  string in case of error. $(NAME:STR) expand STR only if the property
 *  NAME is available.
 *
 *  \param prop_list An array of \ref m_option describing the available
 *                   properties.
 *  \param str The string to expand.
 *  \return The newly allocated expanded string.
 */
char* m_properties_expand_string(m_option_t* prop_list,const char* str, any_t*ctx);

// Helpers to use MPlayer's properties

/// Do an action with an MPlayer property.
int mp_property_do(const char* name,int action, any_t* val, any_t*ctx);

/// Get the value of a property as a string suitable for display in an UI.
char* mp_property_print(const char *name, any_t* ctx);

/// \defgroup PropertyImplHelper Property implementation helpers
/// \ingroup Properties
/// \brief Helper functions for common property types.
///@{

/// Clamp a value according to \ref m_option::min and \ref m_option::max.
static inline double M_PROPERTY_CLAMP(m_option_t* prop,double val) {
    if((prop->flags & M_OPT_MIN) && (val<prop->min))
	val=prop->min;
    else if((prop->flags & M_OPT_MAX) && (val>prop->max))
	val=prop->max;
    return val;
}
/// Implement get.
int m_property_int_ro(m_option_t* prop,int action,
		      any_t* arg,int var);

/// Implement set, get and step up/down.
int m_property_int_range(m_option_t* prop,int action,
			 any_t* arg,int* var);

/// Same as m_property_int_range but cycle.
int m_property_choice(m_option_t* prop,int action,
		      any_t* arg,int* var);

/// Switch betwen min and max.
int m_property_flag(m_option_t* prop,int action,
		    any_t* arg,int* var);

/// Implement get, print.
int m_property_float_ro(m_option_t* prop,int action,
			any_t* arg,float var);

/// Implement set, get and step up/down
int m_property_float_range(m_option_t* prop,int action,
			   any_t* arg,float* var);

/// float with a print function which print the time in ms
int m_property_delay(m_option_t* prop,int action,
		     any_t* arg,float* var);

/// Implement get, print
int m_property_double_ro(m_option_t* prop,int action,
			 any_t* arg,double var);

/// Implement print
int m_property_time_ro(m_option_t* prop,int action,
		       any_t* arg,double var);

/// get/print the string
int m_property_string_ro(m_option_t* prop,int action,any_t* arg, char* str);

/// get/print a bitrate
int m_property_bitrate(m_option_t* prop,int action,any_t* arg,int rate);

///@}

///@}
#endif