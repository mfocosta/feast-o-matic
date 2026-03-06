import json
from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt

# --- Configurações ---
MQTT_BROKER = "localhost"  # ou o IP do broker se não for o mesmo
MQTT_PORT = 1883
TOPICO_TESTE = "test"
TOPICO_TESTE2 = "test2"

app = Flask(__name__)
app.config['SECRET_KEY'] = 'segredo_super_secreto'
socketio = SocketIO(app)
mqtt_client = mqtt.Client()

# --- Callbacks do MQTT ---
def on_connect(client, userdata, flags, rc):
    print(f"Conectado ao MQTT com código: {rc}")
    # Subscreve para ouvir o que o ESP32 diz
    client.subscribe(TOPICO_TESTE)

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        print(f"Recebido do ESP32: {payload}")
        
        # Envia a informação para o navegador em tempo real via SocketIO
        socketio.emit('atualizacao_sensor', {'dados': payload})
    except Exception as e:
        print(f"Erro ao processar mensagem: {e}")

# Configura o MQTT
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start() # Roda o MQTT numa thread separada

# --- Rotas do Flask (Web) ---
@app.route('/')
def index():
    return render_template('index.html')

# --- Eventos do SocketIO (Comandos do Browser) ---
@socketio.on('comando_alimentar')
def handle_feed_command(json_data):
    print("Comando recebido da Web: Alimentar")
    # Publica no MQTT para o ESP32 ouvir
    # Enviamos um JSON, ex: {"acao": "alimentar"}
    mensagem = json.dumps(json_data)
    mqtt_client.publish(TOPICO_TESTE2, mensagem)

if __name__ == '__main__':
    # host='0.0.0.0' permite acesso de outros PCs na rede
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)