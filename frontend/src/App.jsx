import React, { useState, useEffect, useRef } from 'react';
import mqtt from 'mqtt';
import './App.css';

function App() {
  const [client, setClient] = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [isLightOn, setIsLightOn] = useState(false);
  const [status, setStatus] = useState('Disconnected');
  const [config, setConfig] = useState({
    broker: '',
    port: '8884',
    username: '',
    password: '',
    topic: 'wildwaves/light'
  });
  const [showConfig, setShowConfig] = useState(true);

  const clientRef = useRef(null);

  useEffect(() => {
    // Load saved config from localStorage
    const savedConfig = localStorage.getItem('mqttConfig');
    if (savedConfig) {
      const parsed = JSON.parse(savedConfig);
      setConfig(parsed);
    }
  }, []);

  useEffect(() => {
    // Cleanup on unmount
    return () => {
      if (clientRef.current) {
        clientRef.current.end();
      }
    };
  }, []);

  const connectMQTT = () => {
    if (!config.broker || !config.username || !config.password || !config.topic) {
      alert('Please fill in all fields');
      return;
    }

    setStatus('Connecting...');

    const clientId = 'mqtt_light_web_' + Math.random().toString(16).substr(2, 8);
    const connectUrl = `wss://${config.broker}:${config.port}/mqtt`;

    const options = {
      clientId,
      username: config.username,
      password: config.password,
      clean: true,
      reconnectPeriod: 1000,
      connectTimeout: 30 * 1000,
    };

    try {
      const mqttClient = mqtt.connect(connectUrl, options);

      mqttClient.on('connect', () => {
        console.log('Connected to MQTT broker');
        setIsConnected(true);
        setStatus('Connected to HiveMQ Cloud');
        setShowConfig(false);

        // Subscribe to topic
        mqttClient.subscribe(config.topic, (err) => {
          if (!err) {
            console.log('Subscribed to:', config.topic);
          } else {
            console.error('Subscribe error:', err);
          }
        });

        // Save config to localStorage
        localStorage.setItem('mqttConfig', JSON.stringify(config));
      });

      mqttClient.on('error', (err) => {
        console.error('Connection error:', err);
        setStatus('Connection failed: ' + err.message);
        setIsConnected(false);
      });

      mqttClient.on('reconnect', () => {
        setStatus('Reconnecting...');
      });

      mqttClient.on('close', () => {
        setStatus('Disconnected');
        setIsConnected(false);
      });

      mqttClient.on('message', (topic, message) => {
        const payload = message.toString();
        console.log('Received message:', payload, 'on topic:', topic);
        
        if (payload === 'ON' || payload === '1') {
          setIsLightOn(true);
        } else if (payload === 'OFF' || payload === '0') {
          setIsLightOn(false);
        }
      });

      setClient(mqttClient);
      clientRef.current = mqttClient;
    } catch (error) {
      console.error('MQTT connection error:', error);
      setStatus('Connection failed');
    }
  };

  const toggleLight = () => {
    if (!isConnected || !client) {
      alert('Please connect to MQTT broker first');
      return;
    }

    const message = isLightOn ? 'OFF' : 'ON';
    
    client.publish(config.topic, message, (err) => {
      if (err) {
        console.error('Publish error:', err);
      } else {
        console.log('Sent:', message);
        setIsLightOn(!isLightOn);
      }
    });
  };

  const disconnect = () => {
    if (client) {
      client.end();
      setClient(null);
      clientRef.current = null;
      setIsConnected(false);
      setStatus('Disconnected');
      setShowConfig(true);
    }
  };

  const handleConfigChange = (field, value) => {
    setConfig(prev => ({ ...prev, [field]: value }));
  };

  return (
    <div className="App">
      <div className="container">
        <h1>💡 Light Control</h1>
        
        <div className={`status ${isConnected ? 'connected' : 'disconnected'}`}>
          {status}
        </div>

        {!showConfig && (
          <>
            <div className="light-state">
              Light is {isLightOn ? 'ON' : 'OFF'}
            </div>

            <button
              className={`light-button ${isLightOn ? 'on' : 'off'}`}
              onClick={toggleLight}
              disabled={!isConnected}
            >
              {isLightOn ? 'ON' : 'OFF'}
            </button>

            <button className="disconnect-button" onClick={disconnect}>
              Disconnect
            </button>
          </>
        )}

        {showConfig && (
          <div className="config">
            <h3>HiveMQ Cloud Settings</h3>
            <input
              type="text"
              placeholder="Broker URL (e.g., abc123.s1.eu.hivemq.cloud)"
              value={config.broker}
              onChange={(e) => handleConfigChange('broker', e.target.value)}
            />
            <input
              type="number"
              placeholder="WebSocket Port"
              value={config.port}
              onChange={(e) => handleConfigChange('port', e.target.value)}
            />
            <input
              type="text"
              placeholder="Username"
              value={config.username}
              onChange={(e) => handleConfigChange('username', e.target.value)}
            />
            <input
              type="password"
              placeholder="Password"
              value={config.password}
              onChange={(e) => handleConfigChange('password', e.target.value)}
            />
            <input
              type="text"
              placeholder="MQTT Topic"
              value={config.topic}
              onChange={(e) => handleConfigChange('topic', e.target.value)}
            />
            <button onClick={connectMQTT}>Connect</button>
          </div>
        )}
      </div>
    </div>
  );
}

export default App;