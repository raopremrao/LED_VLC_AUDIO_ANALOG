import { Logger } from './logger.js';

export class Player {
    constructor(audioElementId) {
        this.audioElement = document.getElementById(audioElementId);
        this.currentUrl = null;
    }

    load(blob) {
        try {
            if (this.currentUrl) {
                URL.revokeObjectURL(this.currentUrl);
            }
            this.currentUrl = URL.createObjectURL(blob);
            this.audioElement.src = this.currentUrl;
            this.audioElement.load();
            this.audioElement.style.display = 'block';
            Logger.info('Player', 'Audio loaded successfully.');
        } catch (error) {
            Logger.error('Player', `Failed to load audio: ${error.message}`);
        }
    }

    play() {
        if (this.currentUrl) {
            this.audioElement.play().catch(e => {
                Logger.error('Player', `Playback failed: ${e.message}`);
            });
        }
    }

    stop() {
        this.audioElement.pause();
        this.audioElement.currentTime = 0;
    }
}
