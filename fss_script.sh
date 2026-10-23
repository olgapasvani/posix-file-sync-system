# Έλεγχος παραμέτρων
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 -p <path> -c <command>"
    exit 1
fi

case "$4" in listAll)
    cat "$2"
    ;;

  listMonitored)
    if [ ! -f "$path" ]; then
      echo "Log file $path not found."
      exit 1
    fi
    echo "Monitored directories from log: $path"
    grep "Monitoring started for" "$path" | awk -F'[:] ' '{print $3}'
    ;;

  listStopped)
    
    ;;

  purge)
    ;;

  *)
    echo "Unknown command: $command"
    exit 1
    ;;
esac
