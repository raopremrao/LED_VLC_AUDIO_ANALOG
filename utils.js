export class Utils {
    /**
     * Converts a byte size into a human-readable string.
     */
    static formatBytes(bytes, decimals = 2) {
        if (!+bytes) return '0 Bytes';
        const k = 1024;
        const dm = decimals < 0 ? 0 : decimals;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`;
    }

    /**
     * Promisified delay/sleep function.
     */
    static sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Calculates ETA based on current progress.
     */
    static calculateETA(startTime, currentBytes, totalBytes) {
        if (currentBytes === 0) return "Calculating...";
        const elapsed = (performance.now() - startTime) / 1000;
        const rate = currentBytes / elapsed;
        const remainingBytes = totalBytes - currentBytes;
        const remainingTime = remainingBytes / rate;
        
        if (remainingTime < 60) return `${Math.round(remainingTime)}s`;
        return `${Math.floor(remainingTime / 60)}m ${Math.round(remainingTime % 60)}s`;
    }

    /**
     * Triggers a browser download for a given Blob.
     */
    static downloadBlob(blob, filename) {
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.style.display = 'none';
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
    }
}
