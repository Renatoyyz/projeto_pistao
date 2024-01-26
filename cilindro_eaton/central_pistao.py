import serial
import RPi.GPIO as GPIO
import time
import csv
from datetime import datetime

global loc
global pulsos_encoder
global milimetros


GPIO.setmode(GPIO.BCM) 
GPIO.setwarnings(False)

#GPIO.setup(22, GPIO.IN, pull_up_down = GPIO.PUD_UP)
GPIO.setup(22, GPIO.IN, pull_up_down = GPIO.PUD_DOWN)

# Inicializa a porta serial
ser = serial.Serial(
    #port='/dev/ttyUSB0',  # Porta serial padrão no Raspberry Pi 4
    port='/dev/ttyS0',  # Porta serial padrão no Raspberry Pi 4
    baudrate=9600,       # Taxa de baud
    timeout=1,            # Timeout de leitura
    xonxoff=True         # Controle de fluxo por software (XON/XOFF)
)

# Função para enviar e receber dados
def enviar_e_receber_dados(id_byte, comando_byte):
    # Configuração para envio
    time.sleep(0.01)
    ser.write([id_byte, comando_byte])
    dados_recebidos = ser.read_until()
    return dados_recebidos

# Método para salvar os dados em um arquivo CSV
def salvar_em_csv(dados_recebidos):
    with open('dados.csv', 'a', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        # O dado recebido é convertido para string e as aspas duplas são removidas antes de salvar
        caracteres = "\n\'\"[]"
        tabela = str.maketrans("","",caracteres)
        dados_recebidos = dados_recebidos.translate(tabela)
        csv_writer.writerow([dados_recebidos])

# Método para salvar os dados em um arquivo CSV
def salvar_confi(dados_recebidos):
    with open('conf.csv', 'w', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        # O dado recebido é convertido para string e as aspas duplas são removidas antes de salvar
        caracteres = "\n\'\"[]"
        tabela = str.maketrans("","",caracteres)
        dados_recebidos = dados_recebidos.translate(tabela)

        csv_writer.writerow([dados_recebidos])


def ler_config(nome_arquivo='conf.csv'):
    try:
        # Abre o arquivo CSV no modo de leitura
        with open(nome_arquivo, 'r', newline='') as arquivo_csv:
            # Cria um objeto reader
            leitor_csv = csv.reader(arquivo_csv)

            # Lê a primeira linha do arquivo
            linha = next(leitor_csv, None)

            # Verifica se a linha não está vazia
            if linha:
                # Retorna o valor da primeira célula como uma string
                # Divida a string usando o ponto e vírgula como delimitador
                partes = linha[0].split(';')
                # Converta as partes para números inteiros
                numero1 = int(partes[0])
                numero2 = int(partes[1])
                return numero1, numero2
            else:
                print(f'O arquivo {nome_arquivo} está vazio.')
                return None, None
    except FileNotFoundError:
        print(f'O arquivo {nome_arquivo} não foi encontrado.')
        return None
def valor_milimetro(pulsos):
    return round((milimetros*pulsos)/pulsos_encoder,1)

try:
    pulsos_encoder, milimetros = ler_config()
    habilita_rotina=False
    leitura_avanco=True
    leitura_recuo=False

    m_avanco = 0.0
    t_avanco = 0
    m_recuo = 0.0
    t_recuo = 0
    while True:
        # Exemplo: envia ID 0x01, comando 0xAA e recebe dados
        id_enviar = 0x01
        # comando_enviar = 0x02
        if GPIO.input(22) == True:
            habilita_rotina = False
            comando_enviar = input("Digite Comando\n1-Avanço pistão\n2-Recuo Pistão\n3-Calibração Val. min.\n4-Calibração Val. Max.\nLigar Botão e pressionar \"Enter\", inicia a rotina\nMas antes enviar comando 7 para preparar rotina.\n")
            if comando_enviar != "":
                dados_recebidos = enviar_e_receber_dados(id_enviar, int(comando_enviar))
            else:    
                comando_enviar = "5"
                time.sleep(3)
                dados_recebidos = enviar_e_receber_dados(id_enviar, int(comando_enviar))
            
        else:
            habilita_rotina = True
            comando_enviar = "8" #Comando para aquisição de medidas
            dados_recebidos = enviar_e_receber_dados(id_enviar, int(comando_enviar))

        # Exibe os dados recebidos
        if dados_recebidos != b'' :
            try:
                loc = dados_recebidos
                #loc = int(loc)
            except:
                loc = None
                print(f'Valor inconsistente: {loc}')
                continue
            if loc != None:
                if int(comando_enviar) == 3:
                    print(f'Encoder Zerado, favor iniciar a medida...\nValor encoder: {loc}')
                if int(comando_enviar) == 4:
                    val =  input("Favor entrar com valor em milimetros\n")
                    print(f'Valor em milimetros: {val} de {int(loc)} pulsos do encoder\n')
                    dados_recebidos = dados_recebidos.rstrip(b'\n')
                    dados_recebidos = bytes.decode(dados_recebidos) + ';' + val + '\n'
                    salvar_confi(dados_recebidos)
                    pulsos_encoder, milimetros = ler_config()

                if int(comando_enviar) == 5:
                    leitura_avanco = True
                
                if int(comando_enviar) == 6:
                    leitura_recuo = True

                
                if int(comando_enviar) == 8:
                    
                    if leitura_avanco == True:
                        if (loc != b'-1\n') and (loc != b'-2\n') and (loc != b'-3\n'):
                            dados_recebidos = dados_recebidos.rstrip(b'\n')
                            dados_recebidos = bytes.decode(dados_recebidos)
                            valores = dados_recebidos.strip().split(";")
                            if len(valores) > 1:
                                milimetro_avanco = valores[0]
                                tempo_avanco = valores[1]
                                if int(milimetro_avanco) > 0 and int(tempo_avanco) > 0:
                                    m_avanco = valor_milimetro(int(milimetro_avanco))
                                    t_avanco = int(tempo_avanco)
                                    enviar_e_receber_dados(id_enviar, 6)# Envia comando para recuar
                                    leitura_avanco = False
                                    leitura_recuo = True
                        else:
                            print('Rotina não habilitada\nEnviar comando 5.\n')
                    elif leitura_recuo == True:
                        if (loc != b'-1\n') and (loc != b'-2\n') and (loc != b'-3\n'):
                            data_now = datetime.now()
                            dados_recebidos = dados_recebidos.rstrip(b'\n')
                            dados_recebidos = bytes.decode(dados_recebidos)
                            valores = dados_recebidos.strip().split(";")
                            if len(valores) > 1:
                                milimetro_recuo = valores[0]
                                tempo_recuo = valores[1]
                                
                                if int(milimetro_recuo) > 0 and int(tempo_recuo) > 0:
                                    m_recuo = valor_milimetro(int(milimetro_recuo))
                                    t_recuo = int(tempo_recuo)
                                    dados_recebidos = str(m_avanco) + ';' + str(t_avanco) + ';' + str(m_recuo) + ';' + str(t_recuo) + ';' + str(data_now.hour) + ':' + str(data_now.minute) + ':' + str(data_now.second) + ':' + str(data_now.microsecond // 1000)
                                    salvar_em_csv(dados_recebidos)
                                    enviar_e_receber_dados(id_enviar, 5)# Envia comando para avanço
                                    leitura_avanco = True
                                    leitura_recuo = False
                        else:
                            print('Rotina não habilitada\nEnviar comando 5.\n')
            time.sleep(1)  # Aguarda 1 segundo antes de enviar novamente
        else:
            time.sleep(1)  # Aguarda 1 segundo antes de enviar novamente

except KeyboardInterrupt:
    pass

finally:
    ser.close()
