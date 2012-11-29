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
    MPXP_Rc		(*probe)(demuxer_t *d);
			/** Opens stream.
			  * @param d	_this demxuer
			 **/
    demuxer_t*		(*open)(demuxer_t *d);
			/** Reads and demuxes stream.
			 * @param d	_this demuxer
			 * @param ds	pointer to stream associated with demuxer
			 * @return	0 - EOF or no stream found; 1 - if packet was successfully readed */
    int			(*demux)(demuxer_t *d,demux_stream_t *ds);
			/** Seeks within of stream.
			 * @param d 		_thid demuxer
			 * @param rel_seek_secs	position in seconds from begin of stream
			 * @param flags		0x01 - seek from start else seek_cur, 0x02 - rel_seek_secs indicates pos in percents/100 else in seconds
			 * @note		this function is optional and maybe NULL
			**/
    void		(*seek)(demuxer_t *d,const seek_args_t* seeka);
			/** Closes driver
			  * @param d	_this demuxer
			 **/
    void		(*close)(demuxer_t *d);
			/** Control interface to demuxer
			  * @param d	_this demuxer
			  * @param cmd	command to be execute (one of DEMUX_CMD_*)
			  * @param arg	optional arguments for thsis command
			  * @return	one of DEMUX_* states
			 **/
    MPXP_Rc		(*control)(const demuxer_t *d,int cmd,any_t*arg);
};
#endif
