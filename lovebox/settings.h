/*
 *  Lovebox Settings
 *  ---
 *  Configure these to the settings which suit your Lovebox best 
 */

// URL to the gist containing your message
const String url = ""; // ex. /your-GitHub-username/asdf/raw/message

// Amount of time to wait before checking for a new message (in seconds)
const int fetchIntervalSeconds = 30;

// Light sensor threshold for turning the screen on and off
const int lightValueThreshold = 14;

// Interval at which the screen will turn off momentarily in order to accurately check the brightness of the environement
// This could most likely be avoided by simply placing the photoresistor further away from the screen
const int brightnessCheckSeconds = 60;

// Position at which the heart is vertical (this can vary based on servo installation)
const int initialServoPosition = 90;