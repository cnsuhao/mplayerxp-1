#ifndef XMP_ENUMS_H_INCLUDED
#define XMP_ENUMS_H_INCLUDED

typedef enum MPXP_RC_e{
    MPXP_Detach	=2,
    MPXP_True	=1,
    MPXP_Ok	=MPXP_True,
    MPXP_False	=0,
    MPXP_Virus	=MPXP_False,
    MPXP_Unknown=-1,
    MPXP_Error	=-2,
    MPXP_NA	=-3
}MPXP_Rc;

#endif
