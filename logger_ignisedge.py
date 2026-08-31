import serial
import csv
import time
import os
from datetime import datetime

# --- CONFIGURACIÓN ---
PUERTO_SERIAL = '/dev/ttyACM0' 
BAUD_RATE = 115200
NOMBRE_ARCHIVO = 'dataset_curanilahue_baseline.csv'

def iniciar_logger():   
    try:
        # Abrimos la conexión con la placa
        ser = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=2)
        print(f"[*] Conectado exitosamente a la placa en {PUERTO_SERIAL}")
        
        # Verificamos si el archivo ya existe en tu sistema Fedora
        archivo_existe = os.path.isfile(NOMBRE_ARCHIVO)
        
        # mode='a' es APPEND (agrega al final sin sobreescribir lo anterior)
        with open(NOMBRE_ARCHIVO, mode='a', newline='') as file:
            writer = csv.writer(file)
            
            # Solo escribimos el encabezado si el archivo es nuevo
            if not archivo_existe:
                writer.writerow(['Timestamp', 'Temperatura_C', 'Humedad_%', 'Presion_hPa', 'Resistencia_Gas_Ohms'])
                print("[*] Archivo nuevo creado con encabezados limpios.")
            else:
                print("[*] Archivo existente detectado. Retomando la captura de datos...")
            
            print(f"[*] Guardando datos en: {NOMBRE_ARCHIVO} (Presiona Ctrl+C para detener)\n")

            while True:
                if ser.in_waiting > 0:
                    # Leemos la línea. Usamos errors='ignore' por si algún bit llega corrupto por RF
                    linea = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if "Temperatura" in linea or not linea:
                        continue
                    
                    datos_sensor = linea.split(',')
                    
                    # Ahora esperamos exactamente los 4 valores de ayer
                    if len(datos_sensor) == 4:
                        timestamp_actual = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                        
                        # Armamos la fila
                        fila_csv = [timestamp_actual] + datos_sensor
                        
                        # Escribimos en el archivo y forzamos el guardado en el disco
                        writer.writerow(fila_csv)
                        file.flush() 
                        
                        print(f"Guardado OK: {fila_csv}")
                    else:
                        # Si llega basura del aire, la ignoramos sin detener el programa
                        pass

    except serial.SerialException as e:
        print(f"[!] Error de conexión: {e}")
        print("Revisa que el PUERTO_SERIAL sea correcto y que PlatformIO no tenga el Monitor Serial abierto.")
    except KeyboardInterrupt:
        print("\n[*] Recolección de datos detenida de forma segura. Tu dataset está intacto.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == '__main__':
    iniciar_logger()