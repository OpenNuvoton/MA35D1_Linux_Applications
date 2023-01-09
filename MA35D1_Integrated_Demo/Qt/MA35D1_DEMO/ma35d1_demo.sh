#!/bin/bash
CMD=$1

function video_playback(){
    gst-launch-1.0 filesrc location=/opt/video_mp4.mp4 ! qtdemux name=demux  demux.audio_0 ! queue ! decodebin ! audioconvert ! audioresample ! autoaudiosink demux.video_0 ! queue ! decodebin ! nufbdevsink ! fakesink sync=true &
    wait
}

function video_playback_stop(){
    result=$(pidof gst-launch-1.0)
    echo $result
    kill -INT $result
}

function voip_demo(){
     clean_fb_0
     clean_fb_1
     cd /opt
     ./phone_gui -platform linuxfb:fb=/dev/fb1 > /dev/tty40 &
     sleep 0.5
     baresip -f /etc/.baresip < /dev/tty40 &
     
     sip=$(pidof phone_gui)
     
     until [ -z "$sip" ]
        do
            sleep 0.5
            sip=$(pidof phone_gui)
        done   
}

function voip_demo_stop(){
     clean_fb_0
     clean_fb_1
     result=$(pidof baresip)
     echo $result
     kill $result
}

function graphic_demo(){
    result=$(pidof MA35D1_DEMO)
    kill -9 $result
    clean_fb_0
    clean_fb_1
    sleep 0.5
    export  QT_QPA_FB_FORCE_FULLSCREEN=0
    cd /opt/
    ./position_test -platform linuxfb:fb=/dev/fb1 &  

    export  QT_QPA_FB_FORCE_FULLSCREEN=1
    cnt=0
    fuser -k /dev/tty1
    sleep 0.2
    ./animatedtiles &
}

function graphic_demo_stop(){  
     result=$(pidof position_test)
     echo $result
     kill $result
     

    result=$(pidof animatedtiles)
     echo $result
     kill $result 

     clean_fb_0
     clean_fb_1
     sleep 0.5
     
     cd /opt
     ./MA35D1_DEMO &

}

function clean_fb_0(){
    ./fb_clean 0 
}

function clean_fb_1(){
     ./fb_clean 1
}


function load_rtp_fw(){
     echo -n /opt > /sys/module/firmware_class/parameters/path
     sleep 0.5
     echo -n I2S_Codec_PDMA.axf > /sys/class/remoteproc/remoteproc0/firmware
     sleep 0.5
     rtp_stop
}

function rtp_start(){
     result=$(cat /sys/class/remoteproc/remoteproc0/state)
     if [ $result == "offline" ]
     then 
       echo start > /sys/class/remoteproc/remoteproc0/state
     fi    
}

function rtp_stop(){
     result=$(cat /sys/class/remoteproc/remoteproc0/state)
     if [ $result == "running" ]
     then 
       echo stop > /sys/class/remoteproc/remoteproc0/state 
     fi
}

function do_main(){
    case $CMD in
    1*) echo "Video playback"
        video_playback
            ;;
    2*) echo "Stop Video playback"
        video_playback_stop   
            ;;
    3*) echo "Graphic Demo"
        graphic_demo
            ;;
    4*) echo "Stop Graphic Demo"
        graphic_demo_stop
            ;;
    5*) echo "VoIP start"
        voip_demo
            ;;
    6*) echo "VoIP stop"
       voip_demo_stop
            ;;
    7*) echo "Clean FB0"
       clean_fb_0
            ;;
    8*) echo "Clean FB1"
       clean_fb_1
            ;;
    9*) echo "Load RTP FW"
       load_rtp_fw
            ;;
    a*) echo "RTP start..."
       rtp_start
            ;;
    b*) echo "RTP halt..."
       rtp_stop
            ;;

esac
}

do_main