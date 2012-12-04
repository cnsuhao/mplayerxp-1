#ifndef __DEMUXER_INTERNAL_H_INCLUDED
#define __DEMUXER_INTERNAL_H_INCLUDED 1

/** Demuxer's driver interface */
struct demuxer_driver_t
{
    const char*		short_name; /**< for forcing through comand line */
    const char*		name;	/**< Name of driver ("Matroska MKV parser") */
    const char*		defext; /**< Default file extension for this movie type */
    const config_t*	options;/**< Optional: MPlayerXP's option related */
			/** Probing stream.
			  * @param d	_this demuxer
			 **/
    MPXP_Rc		(*probe)(Demuxer *d);
			/** Opens stream.
			  * @param d	_this demxuer
			 **/
    Demuxer*		(*open)(Demuxer *d);
			/** Reads and demuxes stream.
			 * @param d	_this demuxer
			 * @param ds	pointer to stream associated with demuxer
			 * @return	0 - EOF or no stream found; 1 - if packet was successfully readed */
    int			(*demux)(Demuxer *d,Demuxer_Stream *ds);
			/** Seeks within of stream.
			 * @param d 		_thid demuxer
			 * @param rel_seek_secs	position in seconds from begin of stream
			 * @param flags		0x01 - seek from start else seek_cur, 0x02 - rel_seek_secs indicates pos in percents/100 else in seconds
			 * @note		this function is optional and maybe NULL
			**/
    void		(*seek)(Demuxer *d,const seek_args_t* seeka);
			/** Closes driver
			  * @param d	_this demuxer
			 **/
    void		(*close)(Demuxer *d);
			/** Control interface to demuxer
			  * @param d	_this demuxer
			  * @param cmd	command to be execute (one of DEMUX_CMD_*)
			  * @param arg	optional arguments for thsis command
			  * @return	one of DEMUX_* states
			 **/
    MPXP_Rc		(*control)(const Demuxer *d,int cmd,any_t*arg);
};
#endif
