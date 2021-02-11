    /*

    Copyright (C) 2001 Takashi Iwai <tiwai@suse.de>
    Copyright (C) 2004 Allan Sandfeld Jensen <kde@carewolf.com>

    based on audioalsa.cc:
    Copyright (C) 2000,2001 Jozef Kosoru
                            jozef.kosoru@pobox.sk
			  (C) 2000,2001 Stefan Westerfeld
			                stefan@space.twc.de

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * only compile 'alsa' AudioIO class if configure things it is a good idea
 */
#ifdef HAVE_LIBASOUND2

#ifdef HAVE_ALSA_ASOUNDLIB_H
#include <alsa/asoundlib.h>
#elif defined(HAVE_SYS_ASOUNDLIB_H)
#include <sys/asoundlib.h>
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>

#include "debug.h"
#include "audioio.h"
#include "audiosubsys.h"
#include "dispatcher.h"
#include "iomanager.h"

namespace Arts {

class AudioIOALSA : public AudioIO, public IONotify  {
protected:
	int audio_read_fd;
	int audio_write_fd;

        snd_pcm_t *m_pcm_playback;
	snd_pcm_t *m_pcm_capture;
	snd_pcm_format_t m_format;
	int m_period_size, m_periods;
        bool inProgress;
	bool restartIOHandling;

        void startIO();
        int poll2iomanager(int pollTypes);
	int setPcmParams(snd_pcm_t *pcm);
	int watchDescriptor(snd_pcm_t *pcm);

        void notifyIO(int fd, int types);

	int xrun(snd_pcm_t *pcm);
#ifdef HAVE_SND_PCM_RESUME
	int resume(snd_pcm_t *pcm);
#endif

public:
	AudioIOALSA();

	void setParam(AudioParam param, int& value);
	int getParam(AudioParam param);

	bool open();
	void close();
	int read(void *buffer, int size);
	int write(void *buffer, int size);
};

REGISTER_AUDIO_IO(AudioIOALSA,"alsa","Advanced Linux Sound Architecture");
};

using namespace std;
using namespace Arts;

AudioIOALSA::AudioIOALSA()
{
 	param(samplingRate) = 44100;
	paramStr(deviceName) = "default"; // ALSA pcm device name - not file name
	param(fragmentSize) = 2048;
	param(fragmentCount) = 7;
	param(channels) = 2;
	param(direction) = directionWrite;

        param(format) = 16;
	/*
	 * default parameters
	 */
	m_format = SND_PCM_FORMAT_S16_LE;

        m_pcm_playback = NULL;
	m_pcm_capture = NULL;
        inProgress = false;
        restartIOHandling = false;
	audio_read_fd = audio_write_fd = -1;
}

bool AudioIOALSA::open()
{
        string& _error = paramStr(lastError);
	string& _deviceName = paramStr(deviceName);
	int& _channels = param(channels);
	int& _fragmentSize = param(fragmentSize);
	int& _fragmentCount = param(fragmentCount);
	int& _samplingRate = param(samplingRate);
	int& _direction = param(direction);
	int& _format = param(format);

	m_pcm_playback = NULL;
	m_pcm_capture = NULL;

	/* initialize format */
	switch(_format) {
	case 16:	// 16bit, signed little endian
		m_format = SND_PCM_FORMAT_S16_LE;
		break;
	case 17:	// 16bit, signed big endian
		m_format = SND_PCM_FORMAT_S16_BE;
		break;
	case 8:		// 8bit, unsigned
		m_format = SND_PCM_FORMAT_U8;
		break;
	default:	// test later
		m_format = SND_PCM_FORMAT_UNKNOWN;
		break;
	}

	/* open pcm device */
	int err;
	if (_direction & directionWrite) {
		if ((err = snd_pcm_open(&m_pcm_playback, _deviceName.c_str(),
					SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
			_error = "device: ";
			_error += _deviceName.c_str();
			_error += " can't be opened for playback (";
			_error += snd_strerror(err);
			_error += ")";
			return false;
		}
		snd_pcm_nonblock(m_pcm_playback, 0);
	}
	if (_direction & directionRead) {
		if ((err = snd_pcm_open(&m_pcm_capture, _deviceName.c_str(),
					SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
			_error = "device: ";
			_error += _deviceName.c_str();
			_error += " can't be opened for capture (";
			_error += snd_strerror(err);
			_error += ")";
			snd_pcm_close(m_pcm_playback);
			return false;
		}
		snd_pcm_nonblock(m_pcm_capture, 0);
	}

	artsdebug("ALSA driver: %s", _deviceName.c_str());

	/* check device capabilities */
	// checkCapabilities();

	/* set PCM communication parameters */
	if (((_direction & directionWrite) && setPcmParams(m_pcm_playback)) ||
	    ((_direction & directionRead) && setPcmParams(m_pcm_capture))) {
		snd_pcm_close(m_pcm_playback);
		snd_pcm_close(m_pcm_capture);
		return false;
	}

  	artsdebug("buffering: %d fragments with %d bytes "
		  "(audio latency is %1.1f ms)", _fragmentCount, _fragmentSize,
		  (float)(_fragmentSize*_fragmentCount) /
		  (float)(2.0 * _samplingRate * _channels)*1000.0);


	startIO();
        /* restore the format value */
	switch (m_format) {
	case SND_PCM_FORMAT_S16_LE:
		_format = 16;
		break;
	case SND_PCM_FORMAT_S16_BE:
		_format = 17;
		break;
	case SND_PCM_FORMAT_U8:
		_format =  8;
		break;
        default:
            _error = "Unknown PCM format";
            return false;
	}

  	/* start recording */
	if (_direction & directionRead)
		snd_pcm_start(m_pcm_capture);
  	/* enable playing (needed for dmix) */
	if (_direction & directionWrite)
		snd_pcm_start(m_pcm_playback);

  	return true;
}

void AudioIOALSA::close()
{
        arts_debug("Closing ALSA-driver");
	int& _direction = param(direction);
	if ((_direction & directionRead) && m_pcm_capture) {
		(void)snd_pcm_drop(m_pcm_capture);
		(void)snd_pcm_close(m_pcm_capture);
		m_pcm_capture = NULL;
	}
	if ((_direction & directionWrite) && m_pcm_playback) {
		(void)snd_pcm_drop(m_pcm_playback);
		(void)snd_pcm_close(m_pcm_playback);
		m_pcm_playback = NULL;
	}
	Dispatcher::the()->ioManager()->remove(this, IOType::all);
}

void AudioIOALSA::setParam(AudioParam p, int& value)
{
	param(p) = value;
        if (m_pcm_playback != NULL) {
            setPcmParams(m_pcm_playback);
        }
        if (m_pcm_capture != NULL) {
            setPcmParams(m_pcm_capture);
        }
}

int AudioIOALSA::getParam(AudioParam p)
{
	snd_pcm_status_t *status;
	snd_pcm_status_alloca(&status);

	switch(p) {
	case canRead:
		if (! m_pcm_capture) return -1;
		if (snd_pcm_status(m_pcm_capture, status) < 0) {
			arts_warning("Capture channel status error!");
			return -1;
		}
		return snd_pcm_frames_to_bytes(m_pcm_capture, snd_pcm_status_get_avail(status));

	case canWrite:
		if (! m_pcm_playback) return -1;
		if(snd_pcm_status(m_pcm_playback, status) < 0) {
			arts_warning("Playback channel status error!");
			return -1;
		}
		return snd_pcm_frames_to_bytes(m_pcm_playback, snd_pcm_status_get_avail(status));

	case selectFD:
		return -1;
		//return audio_write_fd;
		//return audio_read_fd;

	case autoDetect:
		/*
		 * that the ALSA driver could be compiled doesn't say anything
		 * about whether it will work (the user might be using an OSS
		 * kernel driver) so we'll use a value less than the OSS one
		 * here, because OSS will most certainly work (ALSA's OSS emu)
		 */
		return 5;

	default:
		return param(p);
	}
}

void AudioIOALSA::startIO()
{
        /* watch PCM file descriptor(s) */
	if (m_pcm_playback) {
		audio_write_fd = watchDescriptor(m_pcm_playback);
        }
	if (m_pcm_capture) {
		audio_read_fd = watchDescriptor(m_pcm_capture);
        }

}

int AudioIOALSA::poll2iomanager(int pollTypes)
{
	int types = 0;

	if(pollTypes & POLLIN)
		types |= IOType::read;
	if(pollTypes & POLLOUT)
		types |= IOType::write;
	if(pollTypes & POLLERR)
		types |= IOType::except;

	return types;
}

int AudioIOALSA::watchDescriptor(snd_pcm_t *pcm)
{
        struct pollfd pfds;
        if (snd_pcm_poll_descriptors_count(pcm) != 1) {
		arts_info("Can't handle more than one poll descriptor\n");
		return -1;
        }
        if (snd_pcm_poll_descriptors(pcm, &pfds, 1) != 1) {
		arts_info("Cannot get poll descriptor\n");
		return -1;
	}

        // See though the crack-fumes from the ALSA-developers and try to
        // figure out which way this handle is supposed to be polled.
        int types = poll2iomanager(pfds.events);

        Dispatcher::the()->ioManager()->watchFD(pfds.fd, types, this);

	return pfds.fd;
}

int AudioIOALSA::xrun(snd_pcm_t *pcm)
{
	int err;
	artsdebug("xrun!!\n");
	if ((err = snd_pcm_prepare(pcm)) < 0)
		return err;
	snd_pcm_start(pcm); // ignore error here..
	return 0;
}

#ifdef HAVE_SND_PCM_RESUME
int AudioIOALSA::resume(snd_pcm_t *pcm)
{
	int err;
	artsdebug("resume!\n");
	while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
		sleep(1); /* wait until suspend flag is not released */
	if (err < 0) {
		if ((err = snd_pcm_prepare(pcm)) < 0)
			return err;
		snd_pcm_start(pcm); // ignore error here..
	}
	return 0;
}
#endif

int AudioIOALSA::read(void *buffer, int size)
{
	int frames = snd_pcm_bytes_to_frames(m_pcm_capture, size);
	int length;
	while ((length = snd_pcm_readi(m_pcm_capture, buffer, frames)) < 0) {
		if (length == -EPIPE)
			length = xrun(m_pcm_capture);
#ifdef HAVE_SND_PCM_RESUME
		else if (length == -ESTRPIPE)
			length = resume(m_pcm_capture);
#endif
		if (length < 0) {
			arts_info("Capture error: %s", snd_strerror(length));
			return -1;
		}
	}
	return snd_pcm_frames_to_bytes(m_pcm_capture, length);
}

int AudioIOALSA::write(void *buffer, int size)
{
        // DMix has an annoying habit of returning instantantly on the returned
        // poll-descriptor. So we block here to avoid an infinity loop.
        while(snd_pcm_wait(m_pcm_playback, 1) == 0);

        int frames = snd_pcm_bytes_to_frames(m_pcm_playback, size);
	int length;
	while ((length = snd_pcm_writei(m_pcm_playback, buffer, frames)) < 0) {
		if (length == -EPIPE)
			length = xrun(m_pcm_playback);
#ifdef HAVE_SND_PCM_RESUME
		else if (length == -ESTRPIPE)
			length = resume(m_pcm_playback);
#endif
		if (length < 0) {
			arts_info("Playback error: %s", snd_strerror(length));
			return -1;
		}
	}
	return snd_pcm_frames_to_bytes(m_pcm_playback, length);
}

void AudioIOALSA::notifyIO(int fd, int type)
{
        int todo = 0;

	if(inProgress)
	{
		if(!restartIOHandling)
		{
			Dispatcher::the()->ioManager()->remove(this,IOType::all);
			restartIOHandling = true;
		}
		return;
	}

	// We can't trust the type as ALSA might have read-type events,
	// that are really meant to be write-type event!
        if(fd == audio_write_fd) todo |= AudioSubSystem::ioWrite;
        if(fd == audio_read_fd) todo |= AudioSubSystem::ioRead;

        if (type & IOType::except) todo |= AudioSubSystem::ioExcept;

        restartIOHandling = false;
        inProgress = true;
        AudioSubSystem::the()->handleIO(todo);
        inProgress = false;

        if (restartIOHandling) startIO();
}

int AudioIOALSA::setPcmParams(snd_pcm_t *pcm)
{
	int &_samplingRate = param(samplingRate);
	int &_channels = param(channels);
	int &_fragmentSize = param(fragmentSize);
	int &_fragmentCount = param(fragmentCount);
	string& _error = paramStr(lastError);

	snd_pcm_hw_params_t *hw;
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(pcm, hw);

	if (snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		_error = "Unable to set interleaved!";
		return 1;
	}
	if (m_format == SND_PCM_FORMAT_UNKNOWN) {
		// test the available formats
		// try 16bit first, then fall back to 8bit
		if (! snd_pcm_hw_params_test_format(pcm, hw, SND_PCM_FORMAT_S16_LE))
			m_format = SND_PCM_FORMAT_S16_LE;
		else if (! snd_pcm_hw_params_test_format(pcm, hw, SND_PCM_FORMAT_S16_BE))
			m_format = SND_PCM_FORMAT_S16_BE;
		else if (! snd_pcm_hw_params_test_format(pcm, hw, SND_PCM_FORMAT_U8))
			m_format = SND_PCM_FORMAT_U8;
                else
                        m_format = SND_PCM_FORMAT_UNKNOWN;
	}
	if (snd_pcm_hw_params_set_format(pcm, hw, m_format) < 0) {
		_error = "Unable to set format!";
		return 1;
	}

	unsigned int rate = snd_pcm_hw_params_set_rate_near(pcm, hw, _samplingRate, 0);
	const unsigned int tolerance = _samplingRate/10+1000;
	if (abs((int)rate - (int)_samplingRate) > (int)tolerance) {
		_error = "Can't set requested sampling rate!";
		char details[80];
		sprintf(details," (requested rate %d, got rate %d)",
			_samplingRate, rate);
		_error += details;
		return 1;
  	}
	_samplingRate = rate;

	if (snd_pcm_hw_params_set_channels(pcm, hw, _channels) < 0) {
		_error = "Unable to set channels!";
		return 1;
	}

	m_period_size = _fragmentSize;
	if (m_format != SND_PCM_FORMAT_U8)
		m_period_size <<= 1;
	if (_channels > 1)
	m_period_size /= _channels;
	if ((m_period_size = snd_pcm_hw_params_set_period_size_near(pcm, hw, m_period_size, 0)) < 0) {
		_error = "Unable to set period size!";
		return 1;
	}
	m_periods = _fragmentCount;
        if ((m_periods = snd_pcm_hw_params_set_periods_near(pcm, hw, m_periods, 0)) < 0) {
		_error = "Unable to set periods!";
		return 1;
	}

	if (snd_pcm_hw_params(pcm, hw) < 0) {
		_error = "Unable to set hw params!";
		return 1;
	}

	_fragmentSize = m_period_size;
        _fragmentCount = m_periods;
	if (m_format != SND_PCM_FORMAT_U8)
		_fragmentSize >>= 1;
	if (_channels > 1)
		_fragmentSize *= _channels;

	return 0; // ok, we're ready..
}

#endif /* HAVE_LIBASOUND2 */
