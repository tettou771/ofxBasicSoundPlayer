/*
 * ofxBasicSoundPlayer.cpp
 *
 *  Created on: 25/07/2012
 *      Author: arturo
 */

#include "ofxBasicSoundPlayer.h"
#include "ofSoundUtils.h"
#include "ofSoundStream.h"
#include <float.h>

int ofxBasicSoundPlayer::maxSoundsTotal=128;
int ofxBasicSoundPlayer::maxSoundsPerPlayer=16;

using namespace std;

ofxBasicSoundPlayer::ofxBasicSoundPlayer() {
	volume = 1;
	streaming = false;
	bIsPlaying = false;
	multiplay = false;
	bIsLoaded = false;
	positions.resize(1,0);
	loop = false;
	speed = 1;
	relativeSpeed.resize(1,1);
	volumesLeft.resize(1,1);
	volumesRight.resize(1,1);
	pan = 0;
	maxSounds = maxSoundsPerPlayer;
}

ofxBasicSoundPlayer::~ofxBasicSoundPlayer() {
	unload();
}

bool ofxBasicSoundPlayer::load(const std::filesystem::path& filePath, bool _stream){
	std::unique_lock<std::mutex> lck(mtx);
	bIsPlaying = false;
	bIsLoaded = false;
    ofLogVerbose() << "ofxBasicSoundPlayer::load" << endl << "Loading file: " << filePath.string();
	bIsLoaded = soundFile.load(filePath.string());
	if(!bIsLoaded) return false;

    ofLogVerbose() << "Duration     : " << soundFile.getDuration();
    ofLogVerbose() << "Channels     : " << soundFile.getNumChannels();
    ofLogVerbose() << "SampleRate   : " << soundFile.getSampleRate();
    ofLogVerbose() << "Num Samples  : " << soundFile.getNumSamples();
    ofLogVerbose() << "isCompressed : " << (soundFile.isCompressed()?"YES":"NO");

    
	streaming = _stream;
	if ( streaming ){
		speed = 1;
	}
	
	if(!streaming){
		soundFile.readTo(buffer);
		ofLogVerbose() << "Not streaming; Reading whole file into memory! ";
	}
    playerNumChannels = soundFile.getNumChannels();
    playerSampleRate = soundFile.getSampleRate();
	return true;
}

void ofxBasicSoundPlayer::audioOutBuffersChanged(int nFrames, int nChannels, int sampleRate){
	if(streaming){
		ofLogNotice() << "Resizing buffer ";
		buffer.resize(nFrames*nChannels,0);
	}
	playerNumFrames = nFrames;
	playerNumChannels = nChannels;
	playerSampleRate = sampleRate;
}

void ofxBasicSoundPlayer::unload(){
	std::unique_lock<std::mutex> lck(mtx);
	soundFile.close();
	buffer.clear();
	bIsPlaying = false;
	bIsLoaded = false;
	positions.resize(1,0);
}

void ofxBasicSoundPlayer::play(){
	std::unique_lock<std::mutex> lck(mtx);
	if(!bIsLoaded) return;
	int pos=0;
	float relSpeed = speed*(double(soundFile.getSampleRate())/double(playerSampleRate));
	float left,right;
	ofStereoVolumes(volume,pan,left,right);

	if (multiplay) {
		if(maxSounds>(int)positions.size()){
			positions.push_back(pos);
			relativeSpeed.push_back(relSpeed);
			volumesLeft.push_back(left);
			volumesRight.push_back(right);
		}
	}else{
		if (streaming) {
			soundFile.seekTo(pos);
		}
		positions.back() = pos;
		relativeSpeed.back() = relSpeed;
		volumesLeft.back() = left;
		volumesRight.back() = right;
	}
	bIsPlaying = true;
}

void ofxBasicSoundPlayer::stop(){
	std::unique_lock<std::mutex> lck(mtx);
	bIsPlaying = false;

	if (streaming){
		soundFile.seekTo(0);
	}

}


void ofxBasicSoundPlayer::setVolume(float vol){
	if(!bIsLoaded) return;
	volume = vol;
	ofStereoVolumes(volume,pan,volumesLeft.back(),volumesRight.back());
}

void ofxBasicSoundPlayer::setPan(float _pan){
	if(!bIsLoaded) return;
	pan = _pan;
	ofStereoVolumes(volume,pan,volumesLeft.back(),volumesRight.back());
}

void ofxBasicSoundPlayer::setSpeed(float spd){
	if(!bIsLoaded) return;
	if ( streaming && fabsf(spd-1.0f)<FLT_EPSILON ){
		ofLogWarning("ofxBasicSoundPlayer") << "setting speed is not supported on streaming sounds";
		return;
	}
	speed = spd;
	relativeSpeed.back() = speed*(double(soundFile.getSampleRate())/double(playerSampleRate));
}

void ofxBasicSoundPlayer::setPaused(bool bP){
	if(!bIsLoaded) return;
	bIsPlaying = false;
}

void ofxBasicSoundPlayer::setLoop(bool bLp){
	if(!bIsLoaded) return;
	loop = bLp;
}

void ofxBasicSoundPlayer::setMultiPlay(bool bMp){
	if(!bIsLoaded) return;
	multiplay = bMp;
	if(!multiplay){
		positions.resize(1);
		relativeSpeed.resize(1);
		volumesLeft.resize(1);
		volumesRight.resize(1);
	}
}

void ofxBasicSoundPlayer::setPosition(float pct){
	if(!bIsLoaded) return;
	positions.back() = pct*buffer.getNumFrames();
}

void ofxBasicSoundPlayer::setPositionMS(int ms){
	if(!bIsLoaded) return;
	setPosition(float(ms)/float(buffer.getDurationMS()));
}


float ofxBasicSoundPlayer::getPosition() const{
	if(!bIsLoaded) return 0.f;
	return float(positions.back())/float(buffer.getNumFrames());
}

int ofxBasicSoundPlayer::getPositionMS() const{
	if(!bIsLoaded) return 0;
	return float(positions.back())*1000./buffer.getSampleRate();
}

bool ofxBasicSoundPlayer::isPlaying() const{
	return bIsLoaded && bIsPlaying;
}

bool ofxBasicSoundPlayer::getIsLooping() const{
	return bIsLoaded && loop;
}

float ofxBasicSoundPlayer::getSpeed() const{
	return bIsLoaded && speed;
}

float ofxBasicSoundPlayer::getPan() const{
	return bIsLoaded && pan;
}

bool ofxBasicSoundPlayer::isLoaded() const{
	return bIsLoaded;
}

float ofxBasicSoundPlayer::getVolume() const{
	return volume;
}

unsigned long ofxBasicSoundPlayer::getDurationMS(){
	if(!bIsLoaded) return 0.f;
	return buffer.getDurationMS();
}

void ofxBasicSoundPlayer::updatePositions(int nFrames){
	if(!bIsLoaded) return;
	for(int i=0;i<(int)positions.size();i++){
		// update positions
		positions[i] += nFrames*relativeSpeed[i];
		if(loop){
			positions[i] %= buffer.getNumFrames();
		}else{
			positions[i] = ofClamp(positions[i],0,buffer.getNumFrames());
			// finished?
			if(positions[i]==buffer.getNumFrames()){
				// yes: remove multiplay instances
				if(positions.size()>1){
					positions.erase(positions.begin()+i);
					relativeSpeed.erase(relativeSpeed.begin()+i);
					volumesLeft.erase(volumesLeft.begin()+i);
					volumesRight.erase(volumesRight.begin()+i);
					i--;
				}
			}
		}
	}
	// finished?
	if(!loop && positions.size()==1 && positions[0]==buffer.getNumFrames()){
		bIsPlaying = false;
	}
}

void ofxBasicSoundPlayer::audioOut(ofSoundBuffer& outputBuffer){
	if(!bIsLoaded) return;
	if(bIsPlaying){
		std::unique_lock<std::mutex> lck(mtx);
		int nFrames = outputBuffer.getNumFrames();
		int nChannels = outputBuffer.getNumChannels();
        if (playerNumChannels != nChannels || playerNumFrames != nFrames || playerSampleRate != outputBuffer.getSampleRate()) {
            audioOutBuffersChanged(nFrames, nChannels, outputBuffer.getSampleRate());
        }
		if(streaming){
			int samplesRead = soundFile.readTo(buffer,nFrames);
			if ( samplesRead==0 ){
				bIsPlaying=false;
				soundFile.seekTo(0);
			}
			else{
				buffer.stereoPan(volumesLeft.back(),volumesRight.back());
				newBufferE.notify(this,buffer);
				buffer.addTo(outputBuffer, 0, loop);
			}
		}else{
  	        if (positions.size() == 1 && abs(speed - 1)<FLT_EPSILON) {
                buffer.addTo(outputBuffer,nFrames,nChannels,positions[0],loop);
                outputBuffer.stereoPan(volumesLeft.back(),volumesRight.back());
            }else{
                for(int i=0;i<(int)positions.size();i++){
                    //assert( resampledBuffer.getNumFrames() == bufferSize*relativeSpeed[i] );
                    if(abs(relativeSpeed[i] - 1)<FLT_EPSILON){
                        buffer.copyTo(resampledBuffer,nFrames,nChannels,positions[i],loop);
                    }else{
                    	buffer.resampleTo(resampledBuffer,positions[i],nFrames,relativeSpeed[i],loop, ofSoundBuffer::Linear);
                    }
                    resampledBuffer.stereoPan(volumesLeft[i],volumesRight[i]);
                    newBufferE.notify(this,resampledBuffer);
                    resampledBuffer.addTo(outputBuffer,0,loop);
                }
            }
			updatePositions(nFrames);
		}
	}
}

ofSoundBuffer & ofxBasicSoundPlayer::getCurrentBuffer(){
	if(streaming){
		return buffer;
	}else{
		return resampledBuffer;
	}
}

void ofxBasicSoundPlayer::setMaxSoundsTotal(int max){
	maxSoundsTotal = max;
}

void ofxBasicSoundPlayer::setMaxSoundsPerPlayer(int max){
	maxSoundsPerPlayer = max;
}

void ofxBasicSoundPlayer::setMaxSounds(int max){
	maxSounds = max;
}
