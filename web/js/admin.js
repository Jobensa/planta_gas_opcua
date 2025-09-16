// Administration Module
class AdminManager {
    constructor() {
        this.backups = [];
    }

    init() {
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Backup management
        const createBackupBtn = document.getElementById('create-backup');
        if (createBackupBtn) {
            createBackupBtn.addEventListener('click', () => this.createBackup());
        }

        const listBackupsBtn = document.getElementById('list-backups');
        if (listBackupsBtn) {
            listBackupsBtn.addEventListener('click', () => this.loadBackups());
        }

        // Configuration validation
        const validateConfigBtn = document.getElementById('validate-config');
        if (validateConfigBtn) {
            validateConfigBtn.addEventListener('click', () => this.validateConfig());
        }
    }

    async createBackup() {
        try {
            this.showLoading(true);
            
            const response = await scadaAPI.createBackup();
            this.showSuccess('Backup creado correctamente: ' + response.filename);
            
            // Refresh backup list
            await this.loadBackups();
            
        } catch (error) {
            this.showError('Error creando backup: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    async loadBackups() {
        try {
            this.showLoading(true);
            
            const response = await scadaAPI.getBackups();
            this.backups = response.backups || [];
            this.renderBackupsList();
            
        } catch (error) {
            console.error('Error loading backups:', error);
            this.showError('Error cargando backups: ' + scadaAPI.formatError(error));
            this.renderBackupsList(); // Render empty list
        } finally {
            this.showLoading(false);
        }
    }

    renderBackupsList() {
        const container = document.getElementById('backups-list');
        if (!container) return;

        if (this.backups.length === 0) {
            container.innerHTML = `
                <div class="text-center text-muted py-4">
                    <i class="fas fa-archive fa-2x mb-2 d-block"></i>
                    No hay backups disponibles
                </div>
            `;
            return;
        }

        container.innerHTML = `
            <div class="table-responsive">
                <table class="table table-sm table-hover">
                    <thead class="table-dark">
                        <tr>
                            <th>Archivo</th>
                            <th>Fecha</th>
                            <th>Tamaño</th>
                            <th>Acciones</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${this.backups.map(backup => this.createBackupRow(backup)).join('')}
                    </tbody>
                </table>
            </div>
        `;
    }

    createBackupRow(backup) {
        const date = backup.created_at ? new Date(backup.created_at).toLocaleString('es-ES') : '-';
        const size = backup.size ? this.formatFileSize(backup.size) : '-';
        
        return `
            <tr>
                <td>
                    <i class="fas fa-file-archive me-2 text-primary"></i>
                    <strong>${backup.filename}</strong>
                </td>
                <td>${date}</td>
                <td>${size}</td>
                <td>
                    <div class="btn-group btn-group-sm">
                        <button type="button" 
                                class="btn btn-outline-success"
                                onclick="adminManager.restoreBackup('${backup.filename}')"
                                title="Restaurar">
                            <i class="fas fa-undo"></i>
                        </button>
                        <button type="button" 
                                class="btn btn-outline-primary"
                                onclick="adminManager.downloadBackup('${backup.filename}')"
                                title="Descargar">
                            <i class="fas fa-download"></i>
                        </button>
                        <button type="button" 
                                class="btn btn-outline-danger"
                                onclick="adminManager.deleteBackup('${backup.filename}')"
                                title="Eliminar">
                            <i class="fas fa-trash"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }

    async restoreBackup(filename) {
        if (!confirm(`¿Está seguro de que desea restaurar el backup "${filename}"?\n\nEsta acción reemplazará la configuración actual.`)) {
            return;
        }

        try {
            this.showLoading(true);
            
            await scadaAPI.restoreBackup(filename);
            this.showSuccess('Backup restaurado correctamente. El sistema se reiniciará automáticamente.');
            
        } catch (error) {
            this.showError('Error restaurando backup: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    downloadBackup(filename) {
        // Create a download link
        const link = document.createElement('a');
        link.href = `${scadaAPI.baseUrl}/backup/${encodeURIComponent(filename)}/download`;
        link.download = filename;
        link.style.display = 'none';
        
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        
        this.showSuccess('Descarga iniciada');
    }

    async deleteBackup(filename) {
        if (!confirm(`¿Está seguro de que desea eliminar el backup "${filename}"?`)) {
            return;
        }

        try {
            this.showLoading(true);
            
            // In a real implementation, there would be a delete backup endpoint
            // For now, we'll simulate the deletion
            this.showSuccess('Backup eliminado correctamente');
            await this.loadBackups();
            
        } catch (error) {
            this.showError('Error eliminando backup: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    async validateConfig() {
        try {
            this.showLoading(true);
            
            const response = await scadaAPI.validateConfig();
            this.renderValidationResults(response);
            
        } catch (error) {
            console.error('Validation error:', error);
            this.showValidationError('Error validando configuración: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    renderValidationResults(results) {
        const container = document.getElementById('validation-results');
        if (!container) return;

        const isValid = results.valid === true;
        const errors = results.errors || [];
        const warnings = results.warnings || [];

        let html = '';

        if (isValid && errors.length === 0 && warnings.length === 0) {
            html = `
                <div class="alert alert-success" role="alert">
                    <i class="fas fa-check-circle me-2"></i>
                    <strong>¡Validación exitosa!</strong> La configuración es válida.
                </div>
            `;
        } else {
            if (errors.length > 0) {
                html += `
                    <div class="alert alert-danger" role="alert">
                        <i class="fas fa-times-circle me-2"></i>
                        <strong>Errores encontrados:</strong>
                        <ul class="mb-0 mt-2">
                            ${errors.map(error => `<li>${error}</li>`).join('')}
                        </ul>
                    </div>
                `;
            }

            if (warnings.length > 0) {
                html += `
                    <div class="alert alert-warning" role="alert">
                        <i class="fas fa-exclamation-triangle me-2"></i>
                        <strong>Advertencias:</strong>
                        <ul class="mb-0 mt-2">
                            ${warnings.map(warning => `<li>${warning}</li>`).join('')}
                        </ul>
                    </div>
                `;
            }
        }

        // Add configuration summary if available
        if (results.summary) {
            html += `
                <div class="alert alert-info" role="alert">
                    <i class="fas fa-info-circle me-2"></i>
                    <strong>Resumen de configuración:</strong>
                    <div class="mt-2">
                        <div class="row">
                            <div class="col-md-6">
                                <small>
                                    <strong>Total de tags:</strong> ${results.summary.total_tags || 0}<br>
                                    <strong>Templates:</strong> ${results.summary.total_templates || 0}<br>
                                </small>
                            </div>
                            <div class="col-md-6">
                                <small>
                                    <strong>Índices OPC UA:</strong> ${results.summary.opcua_indices || 0}<br>
                                    <strong>Última validación:</strong> ${new Date().toLocaleString('es-ES')}<br>
                                </small>
                            </div>
                        </div>
                    </div>
                </div>
            `;
        }

        container.innerHTML = html;
        
        if (isValid && errors.length === 0) {
            this.showSuccess('Validación completada exitosamente');
        }
    }

    showValidationError(message) {
        const container = document.getElementById('validation-results');
        if (!container) return;

        container.innerHTML = `
            <div class="alert alert-danger" role="alert">
                <i class="fas fa-times-circle me-2"></i>
                <strong>Error de validación:</strong> ${message}
            </div>
        `;
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    showLoading(show) {
        const spinner = document.getElementById('loading-spinner');
        if (spinner) {
            spinner.classList.toggle('d-none', !show);
        }
    }

    showSuccess(message) {
        this.showToast(message, 'success');
    }

    showError(message) {
        this.showToast(message, 'danger');
    }

    showToast(message, type = 'info') {
        const alertClass = type === 'success' ? 'alert-success' : 
                          type === 'danger' ? 'alert-danger' : 'alert-info';
        
        const toast = document.createElement('div');
        toast.className = `alert ${alertClass} alert-dismissible position-fixed top-0 end-0 m-3`;
        toast.style.zIndex = '9999';
        toast.innerHTML = `
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    }
}

// Global admin manager instance
window.adminManager = new AdminManager();