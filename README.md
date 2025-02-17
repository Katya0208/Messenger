# Клиент-серверное приложение для обмена сообщениями 

Данное приложение представляет собой программу для обмена текстовыми и аудио сообщениями между клиентами. Приложение имеет две ключевые программы: server и client. 

**На сервере**: хранятся все данные подключенных пользователей, создаются и удаляются каналы.

**На клиенте**: осуществляется регистрация и авторизация клиентов, есть возможность подключения к каналам сервера для передачи сообщений другим клиентам и получения сообщений от них.

Приложение поддерживает подключение нескольких пользователей к клиенту.

## Запуск программы

```
path/messaging_app$ make
path/messaging_app/program$ ./server <port>
path/messaging_app/program$ ./client <ip-server:port>
```

## Помощь в использовании

```
path/messaging_app/program$ ./server -h
path/messaging_app/program$ ./client -h
```

Когда клиент уже подключился к серверу можно ввести команду `/help` для вывода более подробной инструкции по использованию

## Примеры работы сервера и клиента

### Подключение сервера
![img](pictures/1.png)

### Подключение и регистрация клиента
![img](pictures/9.png)

### Создание канала на сервере
![img](pictures/3.png)

### Подключение клиента к каналу, отправка сообщений и общение с другими пользователями
![img](pictures/4.png)

### Включение и отключение отображения времени
![img](pictures/5.png)

### Запуск с помощью /connect
![img](pictures/6.png)

### Удаление канала на сервере и отправка оповещений всем пользователям, подключенным к указанному каналу
#### server: 
![img](pictures/7.png)
#### client: 
![img](pictures/8.png)

### Авторизация клиента
![img](pictures/10.png)

### Отправка аудиосообщния в канал
![img](pictures/11.png)

### Прослушивание аудиосообщения из канала и сохранение аудиофайла в формате .wav
![img](pictures/12.png)

### Отправка файла в канал
![img](pictures/17.png)

### Смена ника
![img](pictures/18.png)

## Тесты

```
path/messaging_app$ make
path/messaging_app/program$ ./server <port>
path/messaging_app/tests$ ./send_script -f <file> -t <time delay>
path/messaging_app/tests$ ./listen_script -f <file> -t <time delay>
```

### send_script:
![img](pictures/13.png)

![img](pictures/14.png)

### listen_script:
![img](pictures/15.png)

![img](pictures/16.png)
