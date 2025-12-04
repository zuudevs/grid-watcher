# Grid-Watcher Production Deployment Guide

Complete guide for deploying Grid-Watcher in production environments.

---

## Table of Contents

- [System Requirements](#system-requirements)
- [Pre-Deployment Checklist](#pre-deployment-checklist)
- [Installation](#installation)
- [Configuration](#configuration)
- [Security Hardening](#security-hardening)
- [Performance Tuning](#performance-tuning)
- [Monitoring](#monitoring)
- [Troubleshooting](#troubleshooting)
- [Backup & Recovery](#backup--recovery)

---

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | 2 cores @ 2.0 GHz |
| **RAM** | 1 GB |
| **Disk** | 100 MB (binary + logs) |
| **OS** | Linux 4.15+, Windows Server 2016+ |
| **Network** | 100 Mbps |

### Recommended Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | 4 cores @ 3.0 GHz+ |
| **RAM** | 4 GB |
| **Disk** | 10 GB SSD (for logs) |
| **OS** | Linux 5.4+ (Ubuntu 20.04, CentOS 8) |
| **Network** | 1 Gbps |

### High-Performance Setup

| Component | Requirement |
|-----------|-------------|
| **CPU** | 8+ cores @ 3.5 GHz+ |
| **RAM** | 8 GB+ |
| **Disk** | NVMe SSD |
| **OS** | Linux 5.15+ with real-time kernel |
| **Network** | 10 Gbps |

### Software Dependencies

- **GCC 11+** or **Clang 14+** (for building)
- **CMake 3.20+** (for building)
- **systemd** (for service management on Linux)

---

## Pre-Deployment Checklist

### Planning Phase

- [ ] Identify SCADA network topology
- [ ] List all trusted IPs (SCADA masters, PLCs, RTUs)
- [ ] Determine monitored ports (502 for Modbus, etc.)
- [ ] Define detection thresholds based on normal traffic
- [ ] Plan log storage and retention policy
- [ ] Identify integration points (SIEM, monitoring)

### Testing Phase

- [ ] Test in non-production environment
- [ ] Simulate normal traffic patterns
- [ ] Validate whitelist configuration
- [ ] Test attack detection (DoS, port scan)
- [ ] Measure baseline performance
- [ ] Verify logging and alerting

### Security Review

- [ ] Review security policies
- [ ] Get approval from security team
- [ ] Document incident response procedures
- [ ] Plan for false positive handling
- [ ] Establish escalation procedures

---

## Installation

### Linux Installation

#### Option 1: Build from Source (Recommended)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Clone repository
git clone https://github.com/yourusername/grid-watcher.git
cd grid-watcher

# Build with optimizations
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_LTO=ON \
      -DENABLE_NATIVE=ON \
      -DCMAKE_INSTALL_PREFIX=/opt/grid-watcher \
      ..

# Build
make -j$(nproc)

# Install
sudo make install
```

#### Option 2: Binary Package (Future)

```bash
# Download release
wget https://github.com/yourusername/grid-watcher/releases/download/v2.0.0/grid-watcher-2.0.0-linux-x86_64.tar.gz

# Extract
tar -xzf grid-watcher-2.0.0-linux-x86_64.tar.gz

# Install
sudo cp -r grid-watcher /opt/
```

### Windows Installation

#### Build from Source

```powershell
# Install dependencies (Visual Studio 2022, CMake)
# Clone repository
git clone https://github.com/yourusername/grid-watcher.git
cd grid-watcher

# Build
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

# Install to C:\Program Files\GridWatcher
cmake --install . --config Release --prefix "C:\Program Files\GridWatcher"
```

### Verify Installation

```bash
# Check version
/opt/grid-watcher/bin/grid_watcher --version

# Check help
/opt/grid-watcher/bin/grid_watcher --help

# Test run (should show demo)
/opt/grid-watcher/bin/grid_watcher
```

---

## Configuration

### Configuration File

Create `/etc/grid-watcher/config.json`:

```json
{
  "detection": {
    "dos_packet_threshold": 1000,
    "dos_byte_threshold": 10000000,
    "port_scan_threshold": 10,
    "write_read_ratio_threshold": 5.0,
    "exception_rate_threshold": 10
  },
  "mitigation": {
    "auto_block_enabled": true,
    "auto_block_duration_minutes": 60,
    "max_concurrent_blocks": 1000
  },
  "network": {
    "whitelisted_ips": [
      "192.168.1.10",
      "192.168.1.11"
    ],
    "monitored_ports": [502, 20000]
  },
  "logging": {
    "log_file": "/var/log/grid-watcher/grid_watcher.log",
    "log_level": "INFO",
    "console_output": false
  },
  "performance": {
    "packet_buffer_size": 4096,
    "log_queue_size": 8192,
    "worker_threads": 4
  }
}
```

### Preset Configurations

#### Conservative (Low False Positives)

```json
{
  "detection": {
    "dos_packet_threshold": 2000,
    "port_scan_threshold": 20,
    "write_read_ratio_threshold": 10.0
  },
  "mitigation": {
    "auto_block_duration_minutes": 30
  }
}
```

#### Aggressive (High Security)

```json
{
  "detection": {
    "dos_packet_threshold": 500,
    "port_scan_threshold": 5,
    "write_read_ratio_threshold": 2.0
  },
  "mitigation": {
    "auto_block_duration_minutes": 120
  }
}
```

### Environment-Specific Configuration

#### Development

```bash
export GRID_WATCHER_LOG_LEVEL=DEBUG
export GRID_WATCHER_CONSOLE_OUTPUT=true
```

#### Production

```bash
export GRID_WATCHER_LOG_LEVEL=INFO
export GRID_WATCHER_CONSOLE_OUTPUT=false
export GRID_WATCHER_CONFIG=/etc/grid-watcher/config.json
```

---

## Security Hardening

### User & Permissions

```bash
# Create dedicated user
sudo useradd -r -s /bin/false grid-watcher

# Set ownership
sudo chown -R grid-watcher:grid-watcher /opt/grid-watcher
sudo chown -R grid-watcher:grid-watcher /var/log/grid-watcher
sudo chown -R grid-watcher:grid-watcher /etc/grid-watcher

# Set permissions
sudo chmod 750 /opt/grid-watcher
sudo chmod 640 /etc/grid-watcher/config.json
sudo chmod 750 /var/log/grid-watcher
```

### SELinux Configuration (if enabled)

```bash
# Create SELinux policy
sudo semanage fcontext -a -t bin_t "/opt/grid-watcher/bin(/.*)?"
sudo restorecon -R /opt/grid-watcher

# Allow network access
sudo setsebool -P nis_enabled 1
```

### Firewall Configuration

```bash
# Allow only required ports
sudo firewall-cmd --permanent --add-port=502/tcp  # Modbus
sudo firewall-cmd --reload
```

### Systemd Service Hardening

```ini
[Service]
# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/grid-watcher

# Resource limits
LimitNOFILE=65536
LimitNPROC=512
LimitCPU=infinity

# Restart policy
Restart=on-failure
RestartSec=10s
```

---

## Service Management

### Systemd Service (Linux)

Create `/etc/systemd/system/grid-watcher.service`:

```ini
[Unit]
Description=Grid-Watcher SCADA Security Monitor
After=network.target
Documentation=https://github.com/yourusername/grid-watcher

[Service]
Type=simple
User=grid-watcher
Group=grid-watcher
ExecStart=/opt/grid-watcher/bin/grid_watcher --config /etc/grid-watcher/config.json
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=10s

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/grid-watcher

# Resource limits
LimitNOFILE=65536
LimitNPROC=512

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=grid-watcher

[Install]
WantedBy=multi-user.target
```

### Service Commands

```bash
# Enable service
sudo systemctl enable grid-watcher

# Start service
sudo systemctl start grid-watcher

# Check status
sudo systemctl status grid-watcher

# View logs
sudo journalctl -u grid-watcher -f

# Restart service
sudo systemctl restart grid-watcher

# Stop service
sudo systemctl stop grid-watcher

# Reload configuration
sudo systemctl reload grid-watcher
```

### Windows Service

Use NSSM (Non-Sucking Service Manager):

```powershell
# Install NSSM
choco install nssm

# Create service
nssm install GridWatcher "C:\Program Files\GridWatcher\bin\grid_watcher.exe"
nssm set GridWatcher AppDirectory "C:\Program Files\GridWatcher"
nssm set GridWatcher AppParameters "--config C:\ProgramData\GridWatcher\config.json"

# Start service
nssm start GridWatcher

# Check status
nssm status GridWatcher
```

---

## Performance Tuning

### CPU Affinity

Pin to specific cores for consistent performance:

```bash
# Linux
sudo systemctl set-property grid-watcher.service CPUAffinity=0-3

# Or in service file
[Service]
CPUAffinity=0-3
```

### Memory Configuration

```bash
# Increase memory limits if needed
[Service]
MemoryMax=2G
MemoryHigh=1G
```

### Network Tuning

```bash
# Increase network buffers
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216
sudo sysctl -w net.core.netdev_max_backlog=5000

# Make permanent
echo "net.core.rmem_max = 16777216" | sudo tee -a /etc/sysctl.conf
echo "net.core.wmem_max = 16777216" | sudo tee -a /etc/sysctl.conf
echo "net.core.netdev_max_backlog = 5000" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### Real-Time Priority (optional)

For ultra-low latency:

```bash
# Set real-time priority
sudo chrt -f 50 /opt/grid-watcher/bin/grid_watcher

# Or in service file
[Service]
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=50
```

---

## Monitoring

### Metrics Collection

#### Prometheus Exporter (Future)

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'grid-watcher'
    static_configs:
      - targets: ['localhost:9090']
```

#### Manual Monitoring Script

```bash
#!/bin/bash
# monitor.sh

while true; do
    echo "=== Grid-Watcher Status ==="
    
    # Check if running
    if systemctl is-active --quiet grid-watcher; then
        echo "Status: RUNNING"
    else
        echo "Status: STOPPED"
    fi
    
    # Resource usage
    echo "CPU: $(ps -p $(pgrep grid_watcher) -o %cpu= 2>/dev/null || echo 0)%"
    echo "Memory: $(ps -p $(pgrep grid_watcher) -o %mem= 2>/dev/null || echo 0)%"
    
    # Tail recent threats
    echo "Recent threats:"
    grep "CRITICAL" /var/log/grid-watcher/grid_watcher.log | tail -5
    
    sleep 60
done
```

### Health Checks

```bash
#!/bin/bash
# health_check.sh

# Check if process is running
if ! pgrep -x "grid_watcher" > /dev/null; then
    echo "ERROR: Grid-Watcher not running"
    exit 1
fi

# Check log file for recent activity
if [ $(find /var/log/grid-watcher/grid_watcher.log -mmin -5) ]; then
    echo "OK: Grid-Watcher active"
    exit 0
else
    echo "WARNING: No recent log activity"
    exit 1
fi
```

### Alerting

```bash
#!/bin/bash
# alert.sh

# Check for critical threats
CRITICAL_COUNT=$(grep "CRITICAL" /var/log/grid-watcher/grid_watcher.log | wc -l)

if [ $CRITICAL_COUNT -gt 10 ]; then
    # Send alert (email, SMS, etc.)
    echo "ALERT: $CRITICAL_COUNT critical threats detected" | \
        mail -s "Grid-Watcher Alert" admin@example.com
fi
```

---

## Troubleshooting

### Common Issues

#### Issue: High CPU Usage

**Symptoms**: CPU usage >50% constantly

**Solutions**:
1. Check packet rate: `grep "PPS" /var/log/grid-watcher/grid_watcher.log`
2. Adjust thresholds if too sensitive
3. Ensure native optimizations enabled: `grep march /opt/grid-watcher/build_info.txt`
4. Consider upgrading hardware

#### Issue: High False Positive Rate

**Symptoms**: Many legitimate IPs blocked

**Solutions**:
1. Switch to conservative preset
2. Increase detection thresholds
3. Add trusted IPs to whitelist
4. Review and adjust behavioral thresholds

#### Issue: Service Won't Start

**Symptoms**: `systemctl start` fails

**Solutions**:
```bash
# Check logs
sudo journalctl -u grid-watcher -n 50

# Check configuration
/opt/grid-watcher/bin/grid_watcher --test-config

# Check permissions
ls -la /opt/grid-watcher/bin/grid_watcher
ls -la /var/log/grid-watcher

# Check dependencies
ldd /opt/grid-watcher/bin/grid_watcher
```

#### Issue: Memory Leak

**Symptoms**: Memory usage grows over time

**Solutions**:
```bash
# Monitor memory
watch -n 5 "ps -p $(pgrep grid_watcher) -o %mem=,rss="

# Check for leaks with valgrind
sudo valgrind --leak-check=full /opt/grid-watcher/bin/grid_watcher

# Restart service if needed
sudo systemctl restart grid-watcher
```

### Log Analysis

```bash
# View recent logs
sudo tail -f /var/log/grid-watcher/grid_watcher.log

# Find errors
grep "ERROR" /var/log/grid-watcher/grid_watcher.log

# Count threats by type
grep "CRITICAL" /var/log/grid-watcher/grid_watcher.log | \
    awk '{print $NF}' | sort | uniq -c

# Top blocked IPs
grep "IP blocked" /var/log/grid-watcher/grid_watcher.log | \
    awk '{print $NF}' | sort | uniq -c | sort -rn | head -10
```

---

## Backup & Recovery

### Backup Strategy

#### Configuration Backup

```bash
#!/bin/bash
# backup_config.sh

BACKUP_DIR="/backup/grid-watcher"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup directory
mkdir -p $BACKUP_DIR

# Backup configuration
tar -czf $BACKUP_DIR/config_$DATE.tar.gz \
    /etc/grid-watcher/

# Keep last 30 days
find $BACKUP_DIR -name "config_*.tar.gz" -mtime +30 -delete
```

#### Log Rotation

```bash
# /etc/logrotate.d/grid-watcher

/var/log/grid-watcher/*.log {
    daily
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 grid-watcher grid-watcher
    sharedscripts
    postrotate
        systemctl reload grid-watcher
    endscript
}
```

### Recovery Procedures

#### Restore Configuration

```bash
# Stop service
sudo systemctl stop grid-watcher

# Restore from backup
sudo tar -xzf /backup/grid-watcher/config_YYYYMMDD_HHMMSS.tar.gz -C /

# Restart service
sudo systemctl start grid-watcher
```

#### Factory Reset

```bash
# Stop service
sudo systemctl stop grid-watcher

# Remove all data
sudo rm -rf /var/log/grid-watcher/*
sudo rm -f /etc/grid-watcher/config.json

# Restore default configuration
sudo cp /opt/grid-watcher/etc/config.json.default \
        /etc/grid-watcher/config.json

# Restart service
sudo systemctl start grid-watcher
```

---

## Production Checklist

### Pre-Launch

- [ ] Configuration tested in non-production
- [ ] All trusted IPs whitelisted
- [ ] Detection thresholds validated
- [ ] Service configured and enabled
- [ ] Logging working correctly
- [ ] Monitoring configured
- [ ] Backup strategy implemented
- [ ] Security hardening applied
- [ ] Documentation updated
- [ ] Team trained on operations

### Post-Launch

- [ ] Monitor for first 24 hours
- [ ] Review logs for false positives
- [ ] Adjust thresholds if needed
- [ ] Validate alerting works
- [ ] Document any issues
- [ ] Schedule regular reviews

---

## Support

### Getting Help

- **Documentation**: https://grid-watcher.readthedocs.io
- **Issues**: https://github.com/yourusername/grid-watcher/issues
- **Email**: support@yourdomain.com
- **Community**: https://community.example.com/grid-watcher

### Professional Support

For enterprise support, contact: enterprise@yourdomain.com

---

## Appendix

### Complete Example: Ubuntu 20.04 Deployment

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# 2. Clone and build
git clone https://github.com/yourusername/grid-watcher.git
cd grid-watcher
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON -DENABLE_NATIVE=ON ..
make -j$(nproc)

# 3. Install
sudo make install

# 4. Create user
sudo useradd -r -s /bin/false grid-watcher

# 5. Create directories
sudo mkdir -p /var/log/grid-watcher
sudo mkdir -p /etc/grid-watcher
sudo chown -R grid-watcher:grid-watcher /var/log/grid-watcher

# 6. Create configuration
sudo tee /etc/grid-watcher/config.json > /dev/null << EOF
{
  "detection": {
    "dos_packet_threshold": 1000,
    "port_scan_threshold": 10
  },
  "network": {
    "whitelisted_ips": ["192.168.1.10"],
    "monitored_ports": [502]
  },
  "logging": {
    "log_file": "/var/log/grid-watcher/grid_watcher.log"
  }
}
EOF

# 7. Install systemd service
sudo cp /opt/grid-watcher/etc/grid-watcher.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable grid-watcher
sudo systemctl start grid-watcher

# 8. Verify
sudo systemctl status grid-watcher
sudo tail -f /var/log/grid-watcher/grid_watcher.log
```

---

**Last Updated**: 2024-01-15  
**Version**: 2.0.0